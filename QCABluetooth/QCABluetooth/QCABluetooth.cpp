// QCABluetooth minimal kext skeleton (conceptual).
// Files bundled here: Info.plist (kext), QCABluetooth.cpp (IOKit/USB transport skeleton), Makefile notes.

// ========================= QCABluetooth.cpp =========================
// Minimal C++ skeleton. This is a starting point; you must flesh out error handling,
// USB endpoint discovery, firmware upload, HCI packet parsing and bridging to IOBluetooth.
#include "QCABluetooth.hpp"
#include "hci.hpp"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "qca_firmware.h"
extern const OSSymbol* gIOUSBHostInterfaceClass;

#define super IOService
OSDefineMetaClassAndStructors(QCABluetooth, IOService)

bool QCABluetooth::init(OSDictionary* dict) {
    IOLog("tommybt: init in\n");
    if (!super::init(dict)) return false;
    _device = nullptr;
    _interface = nullptr;
    _workloop = nullptr;
    _cmdGate = nullptr;
    return true;
}

void QCABluetooth::free() {
    if (_cmdGate) {
        _cmdGate->release();
        _cmdGate = nullptr;
    }
    if (_workloop) {
        _workloop->release();
        _workloop = nullptr;
    }
    super::free();
}

IOService* QCABluetooth::probe(IOService* provider, SInt32* score) {
    IOLog("tommybt: probe in\n");
    IOService* res = super::probe(provider, score);
    // Optionally adjust score
    return res;
}

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
    
    
    // Upload firmware if required
    if (! uploadFirmware()) {
        IOLog("tommybt: firmware upload failed (may still work if not required)\n");
        // continue — some devices might not require external firmware
        stop(provider);
        return false;
    }
    
    // 开始监听 Event (interrupt in pipe)
    if (_intInPipe) {
        allocateEventRead();
    } else {
        IOLog("tommybt: no interrupt in pipe found, cannot receive events!\n");
    }


    // TODO: set up USB pipes and callbacks to receive HCI events
    IOLog("tommybt: started\n");
    registerService();
    return true;
}

bool QCABluetooth::findAndOpenInterface() {
    IOLog("tommybt: findAndOpenInterface (compatible -> interface-first)\n");

    if (!_device) {
        IOLog("tommybt: no _device\n");
        return false;
    }

    // 清理旧的
    if (_intInPipe)   { _intInPipe->release();   _intInPipe = nullptr; }
    if (_bulkInPipe)  { _bulkInPipe->release();  _bulkInPipe = nullptr; }
    if (_bulkOutPipe) { _bulkOutPipe->release(); _bulkOutPipe = nullptr; }
    if (_usbInterface){ _usbInterface->close(this); _usbInterface->release(); _usbInterface = nullptr; }

    if (!_device->isOpen(this)) {
        if (!_device->open(this)) {
            IOLog("tommybt: device open failed\n");
            return false;
        }
    }

    // 尝试触发 interface 创建
    const StandardUSB::ConfigurationDescriptor* cfg = _device->getConfigurationDescriptor(0);
    if (cfg) {
        IOLog("tommybt: attempt reset config to force interfaces\n");
        _device->setConfiguration(0);
        IOSleep(100); // give kernel a bit to rebuild config
        IOReturn ok = _device->setConfiguration(cfg->bConfigurationValue, true);
        if (ok != kIOReturnSuccess) {
            IOLog("tommybt: setConfiguration(%u) failed\n", cfg->bConfigurationValue);
        } else {
            IOLog("tommybt: setConfiguration(%u) success\n", cfg->bConfigurationValue);
        }
    } else {
        IOLog("tommybt: getConfigurationDescriptor(0) returned NULL\n");
    }

    USBStatus status;
    IOReturn err = getDeviceStatus(this, &status);
    if (err) {
        IOLog("tommybt: - unable to get device status\n");
        _device->close(this);
        return false;
    }

    bool found = false;
    IOUSBHostInterface* openedIface = nullptr;

    OSIterator* iter = _device->getChildIterator(gIOServicePlane);
    if (!iter) {
        IOLog("tommybt: getChildIterator returned NULL\n");
    } else {
        IOLog("tommybt: scanning child objects for IOUSBHostInterface\n");
        OSObject* candidate = nullptr;
        while ((candidate = iter->getNextObject())) {
            IOUSBHostInterface* iface = OSDynamicCast(IOUSBHostInterface, candidate);
            if (!iface)
                continue;

            uint8_t ifaceClass = 0xFF, ifaceSubClass = 0xFF, ifaceProtocol = 0xFF;
            if (OSNumber* n = OSDynamicCast(OSNumber, iface->getProperty("bInterfaceClass")))
                ifaceClass = (uint8_t)n->unsigned32BitValue();
            if (OSNumber* n = OSDynamicCast(OSNumber, iface->getProperty("bInterfaceSubClass")))
                ifaceSubClass = (uint8_t)n->unsigned32BitValue();
            if (OSNumber* n = OSDynamicCast(OSNumber, iface->getProperty("bInterfaceProtocol")))
                ifaceProtocol = (uint8_t)n->unsigned32BitValue();

            IOLog("tommybt: candidate iface %p class=%u sub=%u proto=%u\n",
                  iface, ifaceClass, ifaceSubClass, ifaceProtocol);

            if (!iface->open(this)) {
                IOLog("tommybt: iface->open failed\n");
                continue;
            }

            openedIface = iface;
            IOSleep(5); // allow endpoints to be ready

            const StandardUSB::ConfigurationDescriptor* cfgDesc = iface->getConfigurationDescriptor();
            const StandardUSB::InterfaceDescriptor* ifDesc = iface->getInterfaceDescriptor();
            if (!cfgDesc || !ifDesc) {
                IOLog("tommybt: interface has NULL descriptor(s)\n");
                iface->close(this);
                openedIface = nullptr;
                continue;
            }

            const StandardUSB::EndpointDescriptor* epDesc = nullptr;
            while ((epDesc = StandardUSB::getNextEndpointDescriptor(cfgDesc, ifDesc, epDesc))) {
                uint8_t epDir  = StandardUSB::getEndpointDirection(epDesc);
                uint8_t epType = StandardUSB::getEndpointType(epDesc);
                uint8_t epAddr = StandardUSB::getEndpointAddress(epDesc);
                uint8_t epNum  = StandardUSB::getEndpointNumber(epDesc);

                IOLog("tommybt: endpoint found addr=0x%02x dir=%u type=%u num=%u\n",
                      epAddr, epDir, epType, epNum);

                if (epDir == kUSBIn && epType == kUSBInterrupt && !_intInPipe) {
                    _intInPipe = iface->copyPipe(epAddr);
                    if (_intInPipe) {
                        _intInPipe->retain();
                        IOLog("tommybt: got interrupt IN pipe (0x%02x)\n", epAddr);
                    }
                } else if (epDir == kUSBIn && epType == kUSBBulk && !_bulkInPipe) {
                    _bulkInPipe = iface->copyPipe(epAddr);
                    if (_bulkInPipe) {
                        _bulkInPipe->retain();
                        IOLog("tommybt: got bulk IN pipe (0x%02x)\n", epAddr);
                    }
                } else if (epDir == kUSBOut && epType == kUSBBulk && !_bulkOutPipe) {
                    _bulkOutPipe = iface->copyPipe(epAddr);
                    if (_bulkOutPipe) {
                        _bulkOutPipe->retain();
                        IOLog("tommybt: got bulk OUT pipe (0x%02x)\n", epAddr);

                        // 直接从 descriptor 提取 wMaxPacketSize 的低 11 位（USB 规范）
                        // (StandardUSB::getEndpointMaxPacketSize 也能算，但需要 device-speed 参数)
                        uint16_t wMax = (uint16_t)(epDesc->wMaxPacketSize & 0x07FF);
                        _bulkOutMaxPacket = wMax;
                        IOLog("tommybt: bulk OUT max packet size = %u\n", _bulkOutMaxPacket);
                    }
                }
            }

            // 检查是否成功拿到必需的 pipe
            if (_intInPipe || _bulkOutPipe) {
                IOLog("tommybt: interface pipes ready (IN/OUT)\n");
                found = true;
                break; // stop scanning more interfaces
            } else {
                IOLog("tommybt: no usable endpoints found in interface\n");
                iface->close(this);
                openedIface = nullptr;
            }
        }
        iter->release();
    }

    if (found && openedIface) {
        _usbInterface = openedIface;
        _usbInterface->retain(); // 保持 interface 打开，直到 stop()
        IOLog("tommybt: retained usb interface %p\n", _usbInterface);
        IOSleep(500);
        IOLog("tommybt: using interface-based pipes\n");
        return true;
    }

    // 清理失败状态
    if (_intInPipe)   { _intInPipe->release();   _intInPipe = nullptr; }
    if (_bulkOutPipe) { _bulkOutPipe->release(); _bulkOutPipe = nullptr; }
    if (openedIface)  { openedIface->close(this); openedIface = nullptr; }

    IOLog("tommybt: failed to create pipes from parsed endpoints\n");
    return false;
}



void QCABluetooth::stop(IOService* provider) {
    IOLog("tommybt: stop\n");
    // Close interface, remove event sources
    if (_workloop && _cmdGate) {
        _workloop->removeEventSource(_cmdGate);
    }
    if (_cmdGate) {
        _cmdGate->release();
        _cmdGate = nullptr;
    }
    if (_workloop) {
        _workloop->release();
        _workloop = nullptr;
    }
    if (_intInPipe)  { _intInPipe->release();  _intInPipe = nullptr; }
        if (_bulkInPipe) { _bulkInPipe->release(); _bulkInPipe = nullptr; }
        if (_bulkOutPipe){ _bulkOutPipe->release(); _bulkOutPipe = nullptr; }

        if (_device && _device->isOpen(this)) {
            _device->close(this);
        }
    super::stop(provider);
}


IOReturn QCABluetooth::getDeviceStatus(IOService* forClient, USBStatus *status)
{
    uint16_t stat       = 0;
    StandardUSB::DeviceRequest request;
    request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeStandard, kRequestRecipientDevice);
    request.bRequest      = kDeviceRequestGetStatus;
    request.wValue        = 0;
    request.wIndex        = 0;
    request.wLength       = sizeof(stat);
    uint32_t bytesTransferred = 0;
    IOReturn result = _device->deviceRequest(forClient, request, &stat, bytesTransferred, kUSBHostStandardRequestCompletionTimeout);
    *status = stat;
    return result;
}

IOReturn QCABluetooth::message(UInt32 type, IOService *provider, void *argument)
{
   // IOLog("tommybt: message type: %d\n", type);
    switch ( type ) {
        case kIOMessageServiceIsTerminated:
            if (_device != NULL && _device->isOpen(this)) {
                IOLog("tommybt: message - service is terminated - closing device\n");
            }
            break;
            
        case kIOMessageServiceIsSuspended:
        case kIOMessageServiceIsResumed:
        case kIOMessageServiceIsRequestingClose:
        case kIOMessageServiceWasClosed:
        case kIOMessageServiceBusyStateChange:
        default:
            break;
    }
    
    return super::message(type, provider, argument);
}

bool QCABluetooth::uploadFirmware() {
    IOLog("tommybt: uploadFirmware start\n");
    
    // 2. main firmware
    IOLog("tommybt: uploading main firmware, size=%lu\n", qca_mainfw_len);
    if (!uploadFirmwareFile_ControlThenBulk(qca_mainfw, qca_mainfw_len)) {
        IOLog("tommybt: main firmware upload failed\n");
        return false;
    }

    // 1. rampatch
    IOLog("tommybt: uploading rampatch, size=%lu\n", qca_rampatch_len);
    if (!uploadFirmwareFile_ControlThenBulk(qca_rampatch, qca_rampatch_len)) {
        IOLog("tommybt: rampatch upload failed\n");
        return false;
    }

    

    IOLog("tommybt: firmware upload done ✅\n");
    return true;
}

// 调试用：发送一个 1 字节的测试包到 bulk out
IOReturn QCABluetooth::sendSmallBulkTest() {
    if (!_bulkOutPipe) {
        IOLog("tommybt: sendSmallBulkTest: no bulkOutPipe\n");
        return kIOReturnNotOpen;
    }

    const uint32_t len = 1;
    IOBufferMemoryDescriptor* buf = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOut);
    if (!buf) {
        IOLog("tommybt: sendSmallBulkTest: alloc fail\n");
        return kIOReturnNoMemory;
    }
    uint8_t* p = (uint8_t*)buf->getBytesNoCopy();
    p[0] = 0xAA;

    IOReturn r = buf->prepare();
    if (r != kIOReturnSuccess) {
        IOLog("tommybt: sendSmallBulkTest: prepare failed 0x%x\n", r);
        buf->release();
        return r;
    }

    IOLog("tommybt: sendSmallBulkTest: writing 1 byte...\n");
    r = _bulkOutPipe->io(buf, len, (IOUSBHostCompletion*)NULL, 0);
    buf->complete();
    buf->release();

    IOLog("tommybt: sendSmallBulkTest: io returned 0x%x\n", r);
    return r;
}

bool QCABluetooth::uploadFirmwareFile_ControlThenBulk(const uint8_t* data, size_t len) {
    if (!_bulkOutPipe) {
        IOLog("tommybt: uploadFirmwareFile_ControlThenBulk: no bulk out pipe\n");
        return false;
    }
    if (!_device) {
        IOLog("tommybt: uploadFirmwareFile_ControlThenBulk: no device\n");
        return false;
    }

    // 下面值需要根据芯片/固件格式调整：
    const size_t FW_HDR_SIZE = 20; // 举例：Ath3k 使用 FW_HDR_SIZE
    const uint8_t VENDOR_REQ = 0x01; // 替换为实际 vendor bRequest
    const uint16_t VENDOR_WVALUE = 0;
    const uint16_t VENDOR_WINDEX = 0;
    const uint32_t CTRL_TIMEOUT = (uint32_t)kUSBHostStandardRequestCompletionTimeout;

    if (len <= FW_HDR_SIZE) {
        IOLog("tommybt: firmware too small\n");
        return false;
    }

    // 1) 发送 control header（Vendor OUT Device Request）
    StandardUSB::DeviceRequest ctlreq;
    ctlreq.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionOut, kRequestTypeVendor, kRequestRecipientDevice);
    ctlreq.bRequest = VENDOR_REQ; // replace with actual
    ctlreq.wValue = VENDOR_WVALUE;
    ctlreq.wIndex = VENDOR_WINDEX;
    ctlreq.wLength = (uint16_t)FW_HDR_SIZE;

    uint32_t bytesTransferred = 0;
    IOReturn r = _device->deviceRequest(this, ctlreq, (void*)data, bytesTransferred, CTRL_TIMEOUT);
    IOLog("tommybt: deviceRequest(header) returned 0x%x bytes=%u\n", r, bytesTransferred);
    if (r != kIOReturnSuccess) {
        IOLog("tommybt: control header failure\n");
        return false;
    }

    // 2) 分块发送剩余固件
    const uint32_t BULK_CHUNK = (_bulkOutMaxPacket > 0) ? _bulkOutMaxPacket : 1024;
    size_t offset = FW_HDR_SIZE;
    int blockIdx = 0;

    while (offset < len) {
        size_t toSend = (len - offset) < BULK_CHUNK ? (len - offset) : BULK_CHUNK;

        IOBufferMemoryDescriptor* membuf = IOBufferMemoryDescriptor::withCapacity((uint32_t)toSend, kIODirectionOut);
        if (!membuf) {
            IOLog("tommybt: failed to alloc membuf\n");
            return false;
        }
        memcpy(membuf->getBytesNoCopy(), data + offset, toSend);

        IOReturn prep = membuf->prepare();
        if (prep != kIOReturnSuccess) {
            IOLog("tommybt: membuf prepare failed 0x%x\n", prep);
            membuf->release();
            return false;
        }

        IOReturn ret = _bulkOutPipe->io(membuf, (uint32_t)toSend, (IOUSBHostCompletion*)NULL, 0);
        membuf->complete();
        membuf->release();

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: bulk write block %d failed 0x%x (toSend=%u)\n", blockIdx, ret, (uint32_t)toSend);
            // 尝试清理并重试一次
            IOLog("tommybt: attempting clear/abort then retry once\n");
            // tryClearAndRetryBulk(); // 可选：会调用 sendSmallBulkTest，或你实现 clear/abort
            // 这里简单返回失败，让外层处理
            return false;
        }

        offset += toSend;
        blockIdx++;
    }

    IOLog("tommybt: uploadFirmwareFile_ControlThenBulk: complete\n");
    return true;
}



bool QCABluetooth::uploadFirmwareFile(const uint8_t* data, size_t len) {
    // sendSmallBulkTest();
    size_t offset = 0;

    while (offset < len) {
        if (offset + 3 > len) break;

        uint16_t opcode = (uint16_t)(data[offset] | (data[offset+1] << 8));
        uint8_t plen = data[offset+2];
        if (offset + 3 + plen > len) break;

        const uint8_t* payload = &data[offset+3];
        size_t cmdLen = 4 + plen; // HCI: 0x01 + opcode(2) + plen + payload

        IOBufferMemoryDescriptor* outBuf = IOBufferMemoryDescriptor::withCapacity(cmdLen, kIODirectionOut);
        if (!outBuf) {
            IOLog("tommybt: failed to alloc outBuf for fw\n");
            break;
        }

        // 填充 HCI 命令包
        void* outPtr = outBuf->getBytesNoCopy();
        uint8_t* p = (uint8_t*)outPtr;
        p[0] = 0x01;               // HCI command packet indicator
        p[1] = data[offset];       // opcode LSB
        p[2] = data[offset+1];     // opcode MSB
        p[3] = plen;
        if (plen)
            memcpy(&p[4], payload, plen);

        // prepare for DMA/IO
        IOReturn prep = outBuf->prepare();
        if (prep != kIOReturnSuccess) {
            IOLog("tommybt: outBuf prepare failed 0x%x\n", prep);
            outBuf->release();
            break;
        }

        // 同步写入（仿照 Ath3k：io(..., completion==NULL, 0)）
        IOReturn ret = _bulkOutPipe->io(outBuf, (uint32_t)cmdLen, (IOUSBHostCompletion*)NULL, 0);

        outBuf->complete();
        outBuf->release();

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: bulkOut io failed ret=0x%x\n", ret);
            break;
        }

        // 注意：不要在这里做同步的 int-in 阻塞读取。
        // Command Complete / Events 应该由 allocateEventRead() 提交的异步读和 sEventReadComplete 回调去处理并去匹配 opcode。
        // 如果你需要同步等待特定 opcode 的回报，请在回调中 signal/wakeup 一个 condition 并在这里等待该 condition（需要小心不要死锁 commandGate）。

        offset += 3 + plen;
    }

    return (offset >= len);
}



// 处理 USB 接收到的数据 (HCI Event)
void QCABluetooth::handleUSBData(uint8_t* data, size_t len) {
    if (len < sizeof(hci_event_hdr)) {
        IOLog("tommybt: HCI event too short\n");
        return;
    }

    hci_event_hdr* hdr = (hci_event_hdr*)data;

    switch (hdr->evt) {
        case EVT_CMD_COMPLETE: {
            if (hdr->plen < 3) {
                IOLog("tommybt: CMD_COMPLETE event too short\n");
                return;
            }
            uint16_t opcode = data[3] | (data[4] << 8);
            uint8_t status  = data[5];

            IOLog("tommybt: CMD_COMPLETE opcode=0x%04X status=0x%02X\n", opcode, status);
            if (status != 0x00) {
                IOLog("tommybt: WARN - command failed!\n");
            }
            break;
        }

        case EVT_CMD_STATUS: {
            if (hdr->plen < 4) {
                IOLog("tommybt: CMD_STATUS event too short\n");
                return;
            }
            uint8_t status     = data[2];
            uint16_t opcode    = data[4] | (data[5] << 8);

            IOLog("tommybt: CMD_STATUS opcode=0x%04X status=0x%02X\n", opcode, status);
            if (status != 0x00) {
                IOLog("tommybt: WARN - command failed!\n");
            }
            break;
        }

        case EVT_VENDOR_SPEC: {
            IOLog("tommybt: Vendor event received, len=%d\n", hdr->plen);
            break;
        }

        default:
            IOLog("tommybt: Unhandled HCI event 0x%02X len=%d\n", hdr->evt, hdr->plen);
            break;
    }
}

// ==================== 异步事件监听 ====================

// Trampoline for IOUSBHostCompletion (C function pointer)
static void sEventReadComplete(void* owner,
                               void* parameter,
                               IOReturn status,
                               uint32_t bytesTransferred) {
    QCABluetooth* self = (QCABluetooth*)owner;
    if (self) {
        self->eventReadComplete(owner, parameter, status, bytesTransferred);
    }
}



bool QCABluetooth::submitAsyncRead() {
    if (!_intInPipe) return false;

    // 分配 buffer
    IOBufferMemoryDescriptor* bufDesc =
        IOBufferMemoryDescriptor::withCapacity(260, kIODirectionIn);
    if (!bufDesc) return false;

    // 准备 completion
    IOUSBHostCompletion completion;
    bzero(&completion, sizeof(completion));
    completion.owner     = this;
    completion.action    = sEventReadComplete; // static callback
    completion.parameter = bufDesc;             // 在回调里负责 release(bufDesc)

    // ---- 正确的调用（内核 kext） ----
    // 注意参数顺序：buffer, size, completion, timeout
    IOReturn ret = _intInPipe->io(bufDesc,
                                  (unsigned int)bufDesc->getLength(),
                                  &completion,
                                  0 /* timeout ms; 0 = default/none */);
    if (ret != kIOReturnSuccess) {
        IOLog("tommybt: submitAsyncRead failed 0x%x\n", ret);
        bufDesc->release();
        return false;
    }

    return true;
}

void QCABluetooth::allocateEventRead() {
    const int bufSize = 260; // 足够存放 HCI event
    _eventBuffer = IOBufferMemoryDescriptor::withCapacity(bufSize, kIODirectionIn);
    if (!_eventBuffer) {
        IOLog("tommybt: failed to allocate event buffer\n");
        return;
    }

    IOUSBHostCompletion completion;
    completion.owner     = this;
    completion.action    = sEventReadComplete;
    completion.parameter = nullptr; // use member _eventBuffer

    IOReturn ret = _intInPipe->io(_eventBuffer, bufSize, &completion, 0);
    if (ret != kIOReturnSuccess) {
        IOLog("tommybt: failed to submit event read: 0x%x\n", ret);
    } else {
        IOLog("tommybt: submitted event read\n");
    }
}

// ========================
// 回调：收到事件
// ========================
void QCABluetooth::eventReadComplete(void* owner,
                                     void* parameter,
                                     IOReturn status,
                                     uint32_t bytesTransferred) {
    QCABluetooth* self = (QCABluetooth*)owner;
    if (!self) return;

    if (status != kIOReturnSuccess || bytesTransferred == 0) {
        IOLog("tommybt: eventReadComplete error status=0x%x\n", status);
        // Release per-transfer buffer if we passed one in parameter
        if (parameter) {
            IOBufferMemoryDescriptor* bufDesc = (IOBufferMemoryDescriptor*)parameter;
            bufDesc->release();
        }
        return;
    }

    // Select the right buffer: per-transfer (parameter) or the member buffer
    IOBufferMemoryDescriptor* bufDesc =
        parameter ? (IOBufferMemoryDescriptor*)parameter : self->_eventBuffer;

    uint8_t* data = (uint8_t*)bufDesc->getBytesNoCopy();
    self->handleUSBData(data, bytesTransferred);

    // Release per-transfer buffer if used
    if (parameter) {
        bufDesc->release();
    }

    // 继续监听下一条（using the member-backed buffer path）
    self->allocateEventRead();
}

