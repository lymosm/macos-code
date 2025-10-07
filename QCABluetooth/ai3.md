黑苹果开发高通蓝牙kext，以下是start函数和findAndOpenInterface函数：
bool QCABluetooth::start(IOService* provider) {
    IOLog("tommybt: start in\n");
    if (!super::start(provider)) return false;

    _device = OSDynamicCast(IOUSBHostDevice, provider);
    if (!_device) {
        IOLog("tommybt: provider is not IOUSBHostDevice\n");
        return false;
    }

    _workloop = IOWorkLoop::workLoop();
    if (!_workloop) {
        IOLog("tommybt: failed to create workloop\n");
        return false;
    }

    _cmdGate = IOCommandGate::commandGate(this);
    if (!_cmdGate) {
        IOLog("tommybt: failed to create command gate\n");
        _workloop->release();
        _workloop = nullptr;
        return false;
    }

    if (_workloop->addEventSource(_cmdGate) != kIOReturnSuccess) {
        IOLog("tommybt: failed to add command gate to workloop\n");
        _cmdGate->release();
        _cmdGate = nullptr;
        _workloop->release();
        _workloop = nullptr;
        return false;
    }

    // Find and open the correct USB interface (bulk/interrupt endpoints)
    if (!findAndOpenInterface()) {
        IOLog("tommybt: findAndOpenInterface failed\n");
        stop(provider);
        return false;
    }
    
    // 开始监听 Event (interrupt in pipe)
    if (_intInPipe) {
        allocateEventRead();
    } else {
        IOLog("tommybt: no interrupt in pipe found, cannot receive events!\n");
    }

    // Upload firmware if required
    if (!uploadFirmware()) {
        IOLog("tommybt: firmware upload failed (may still work if not required)\n");
        // continue — some devices might not require external firmware
    }

    // TODO: set up USB pipes and callbacks to receive HCI events
    IOLog("tommybt: started\n");
    registerService();
    return true;
}

bool QCABluetooth::findAndOpenInterface() {
    IOLog("tommybt: findAndOpenInterface (registry-property + pipe enum)\n");

    if (!_device) {
        IOLog("tommybt: no device\n");
        return false;
    }

    // 先释放之前残留的 pipe（若有）
    if (_intInPipe)  { _intInPipe->release();  _intInPipe = nullptr; }
    if (_bulkInPipe) { _bulkInPipe->release(); _bulkInPipe = nullptr; }
    if (_bulkOutPipe){ _bulkOutPipe->release(); _bulkOutPipe = nullptr; }

    // 枚举 device 的所有子节点（使用 gIOServicePlane，然后通过 OSDynamicCast 筛选 IOUSBHostInterface）
    OSIterator* iter = _device->getChildIterator(gIOServicePlane);
    if (!iter) {
        IOLog("tommybt: getChildIterator(gIOServicePlane) failed\n");
        return false;
    }

    IOUSBHostInterface* iface = nullptr;
    bool found = false;

    while ((iface = OSDynamicCast(IOUSBHostInterface, iter->getNextObject()))) {
        // 尝试打开接口
        IOReturn ret = iface->open(this);
        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: iface->open failed 0x%x\n", ret);
            continue;
        }

        // ---- 通过 IORegistry 属性读取接口的 class/subclass/protocol（避免 CopyDescriptors） ----
        uint8_t ifaceClass = 0xFF, ifaceSubClass = 0xFF, ifaceProtocol = 0xFF;
        if (OSObject* o = iface->getProperty("bInterfaceClass")) {
            if (OSNumber* n = OSDynamicCast(OSNumber, o))
                ifaceClass = (uint8_t)n->unsigned32BitValue();
        }
        if (OSObject* o = iface->getProperty("bInterfaceSubClass")) {
            if (OSNumber* n = OSDynamicCast(OSNumber, o))
                ifaceSubClass = (uint8_t)n->unsigned32BitValue();
        }
        if (OSObject* o = iface->getProperty("bInterfaceProtocol")) {
            if (OSNumber* n = OSDynamicCast(OSNumber, o))
                ifaceProtocol = (uint8_t)n->unsigned32BitValue();
        }

        IOLog("tommybt: iface props: class=%u sub=%u proto=%u\n",
              ifaceClass, ifaceSubClass, ifaceProtocol);

        // 如果你想只匹配特定 class/subclass，可以在这里加判断：
        // if (ifaceClass != YOUR_WANTED_CLASS) { iface->close(this); continue; }

        // ---- 枚举该 interface 的子节点，寻找 IOUSBHostPipe ----
        OSIterator* pipeIter = iface->getChildIterator(gIOServicePlane);
        if (pipeIter) {
            IOUSBHostPipe* pipe = nullptr;
            while ((pipe = OSDynamicCast(IOUSBHostPipe, pipeIter->getNextObject()))) {
                // getEndpointDescriptor() 在 IOUSBHostPipe 上通常可用（返回 EndpointDescriptor）
                const StandardUSB::EndpointDescriptor* epd = pipe->getEndpointDescriptor();
                if (!epd) continue;

                uint8_t epAddr = epd->bEndpointAddress;
                uint8_t epType = epd->bmAttributes & kUSBTransferTypeMask;

                IOLog("tommybt: endpoint 0x%02x type=%u maxPacket=%u\n",
                      epAddr, epType, USBToHost16(epd->wMaxPacketSize));

                // 根据方向和类型保存 pipe
                if ((epAddr & kUSBbEndpointDirectionMask) == kUSBIn) {
                    if (epType == kUSBInterrupt && !_intInPipe) {
                        _intInPipe = pipe;
                        _intInPipe->retain();
                        IOLog("tommybt: found interrupt IN pipe\n");
                    } else if (epType == kUSBBulk && !_bulkInPipe) {
                        _bulkInPipe = pipe;
                        _bulkInPipe->retain();
                        IOLog("tommybt: found bulk IN pipe\n");
                    }
                } else { // OUT
                    if (epType == kUSBBulk && !_bulkOutPipe) {
                        _bulkOutPipe = pipe;
                        _bulkOutPipe->retain();
                        IOLog("tommybt: found bulk OUT pipe\n");
                    }
                }
            } // while pipe
            pipeIter->release();
        } // if pipeIter

        // 如果已经找齐必须的 pipe（interrupt IN + bulk OUT），就成功
        if (_intInPipe && _bulkOutPipe) {
            found = true;
            IOLog("tommybt: required pipes found (int in + bulk out)\n");
            // 注意：我们保留了 pipe（retain），因此可以在 stop()/free() 中释放；是否 close iface 视你的设计而定
            break;
        }

        // 未找到则关闭该 iface，继续下一个
        iface->close(this);
    } // while iface

    iter->release();

    if (!found) {
        IOLog("tommybt: missing required pipes (bulk out / int in)\n");
        if (_intInPipe)  { _intInPipe->release();  _intInPipe = nullptr; }
        if (_bulkInPipe) { _bulkInPipe->release(); _bulkInPipe = nullptr; }
        if (_bulkOutPipe){ _bulkOutPipe->release(); _bulkOutPipe = nullptr; }
        return false;
    }

    IOLog("tommybt: interface + pipes ready\n");
    return true;
}

已知：蓝牙USB口：如下：
+-o IOUSBHostDevice@14700000  <class IOUSBHostDevice, id 0x1000003bf, registered, matched, active, busy 0 (1 ms), retain 13>
    |   {
    |     "sessionID" = 5441522528
    |     "USBSpeed" = 1
    |     "idProduct" = 12312
    |     "iManufacturer" = 0
    |     "bDeviceClass" = 224
    |     "IOPowerManagement" = {"PowerOverrideOn"=Yes,"DevicePowerState"=2,"CurrentPowerState"=2,"CapabilityFlags"=32768,"MaxPowerState"=2,"DriverPowerState"=0}
    |     "bcdDevice" = 1
    |     "bMaxPacketSize0" = 64
    |     "iProduct" = 0
    |     "iSerialNumber" = 0
    |     "bNumConfigurations" = 1
    |     "UsbDeviceSignature" = <ca0418300100e00101e00101>
    |     "USB Address" = 4
    |     "locationID" = 342884352
    |     "bDeviceSubClass" = 1
    |     "bcdUSB" = 272
    |     "IOCFPlugInTypes" = {"9dc7b780-9ec0-11d4-a54f-000a27052861"="IOUSBHostFamily.kext/Contents/PlugIns/IOUSBLib.bundle"}
    |     "bDeviceProtocol" = 1
    |     "USBPortType" = 2
    |     "IOServiceDEXTEntitlements" = (("com.apple.developer.driverkit.transport.usb","com.apple.developer.driverkit.builtin"))
    |     "Device Speed" = 1
    |     "idVendor" = 1226
    |     "kUSBAddress" = 4
    |   }
USB树中这个口只有IOUSBHostDevice这一个，没有其他层级了。

问题：为何日志打印了：tommybt: missing required pipes (bulk out / int in) 而且还没有进入：while ((iface = OSDynamicCast(IOUSBHostInterface, iter->getNextObject()))) { 循环？
