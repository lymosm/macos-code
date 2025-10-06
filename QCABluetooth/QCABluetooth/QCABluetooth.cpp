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
    super::stop(provider);
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


bool QCABluetooth::uploadFirmware() {
    IOLog("tommybt: uploadFirmware start\n");

    // 1. rampatch
    IOLog("tommybt: uploading rampatch, size=%lu\n", qca_rampatch_len);
    if (!uploadFirmwareFile(qca_rampatch, qca_rampatch_len)) {
        IOLog("tommybt: rampatch upload failed\n");
        return false;
    }

    // 2. main firmware
    IOLog("tommybt: uploading main firmware, size=%lu\n", qca_mainfw_len);
    if (!uploadFirmwareFile(qca_mainfw, qca_mainfw_len)) {
        IOLog("tommybt: main firmware upload failed\n");
        return false;
    }

    IOLog("tommybt: firmware upload done ✅\n");
    return true;
}


bool QCABluetooth::uploadFirmwareFile(const uint8_t* data, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        if (offset + 3 > len) break;

        uint16_t opcode = data[offset] | (data[offset+1] << 8);
        uint8_t plen = data[offset+2];
        if (offset + 3 + plen > len) break;

        const uint8_t* payload = &data[offset+3];
        uint8_t buf[260];
        buf[0] = 0x01;                // HCI command packet indicator
        buf[1] = data[offset];
        buf[2] = data[offset+1];
        buf[3] = plen;
        memcpy(&buf[4], payload, plen);
        size_t cmdLen = 4 + plen;

        IOReturn ret;
        IOMemoryDescriptor* md = nullptr;

        // --- 发送到 bulk out ---
        md = IOMemoryDescriptor::withAddress(buf, cmdLen, kIODirectionOut);
        if (!md) {
            IOLog("tommybt: failed to create output memory descriptor\n");
            break;
        }
        uint32_t bytesWritten = 0;
        ret = _bulkOutPipe->io(md, (uint32_t)cmdLen, bytesWritten, 5000);
        md->release();

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: bulkOut io failed, ret=0x%x\n", ret);
            break;
        }

        // --- 等待 Command Complete ---
        uint8_t evtBuf[260];
        uint32_t evtLen = sizeof(evtBuf);
        md = IOMemoryDescriptor::withAddress(evtBuf, evtLen, kIODirectionIn);
        if (!md) {
            IOLog("tommybt: failed to create input memory descriptor\n");
            break;
        }
        uint32_t bytesRead = 0;
        ret = _intInPipe->io(md, evtLen, bytesRead, 5000);
        md->release();

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: intIn io failed, ret=0x%x\n", ret);
            break;
        }

        if (bytesRead > 0 && evtBuf[0] == 0x0E) {
            if (bytesRead >= 6) {
                uint16_t rspOpcode = evtBuf[4] | (evtBuf[5] << 8);
                if (rspOpcode != opcode) {
                    IOLog("tommybt: opcode mismatch 0x%04x vs 0x%04x\n", rspOpcode, opcode);
                }
            }
        } else if (bytesRead > 0) {
            IOLog("tommybt: got non-CC event 0x%02x\n", evtBuf[0]);
        }

        offset += 3 + plen;
    }

    return (offset >= len);
}


/*
bool QCABluetooth::uploadFirmware --old() {
    IOLog("tommybt: uploadFirmware start\n");

    OSData* fwData = OSData::withBytes(qca_firmware_hcd, qca_firmware_hcd_len);
    if (!fwData) {
        IOLog("tommybt: firmware could not be loaded into OSData\n");
        return false;
    }

    const uint8_t* data = (const uint8_t*)fwData->getBytesNoCopy();
    size_t len = fwData->getLength();
    IOLog("tommybt: firmware size = %lu\n", len);

    size_t offset = 0;
    while (offset < len) {
        if (offset + 3 > len) break;

        uint16_t opcode = data[offset] | (data[offset+1] << 8);
        uint8_t plen = data[offset+2];
        if (offset + 3 + plen > len) break;

        const uint8_t* payload = &data[offset+3];
        uint8_t buf[260];
        buf[0] = 0x01;
        buf[1] = data[offset];
        buf[2] = data[offset+1];
        buf[3] = plen;
        memcpy(&buf[4], payload, plen);
        size_t cmdLen = 4 + plen;

        IOReturn ret;
        IOMemoryDescriptor* md = nullptr;

        // --- 3. 发送到 bulk out (使用正确的同步 io 方法) ---
        
        // 创建 IOMemoryDescriptor 来包装我们的发送缓冲区
        md = IOMemoryDescriptor::withAddress(buf, cmdLen, kIODirectionOut);
        if (!md) {
            IOLog("tommybt: failed to create output memory descriptor\n");
            break;
        }

        uint32_t bytesWritten = 0; // 这个变量在这里没用，但函数需要它
        ret = _bulkOutPipe->io(md, (uint32_t)cmdLen, bytesWritten, 5000); // 5000ms timeout
        md->release(); // 必须释放 IOMemoryDescriptor

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: bulkOut io failed, ret=0x%x\n", ret);
            break;
        }

        // --- 4. 阻塞等待 Command Complete (使用正确的同步 io 方法) ---
        uint8_t evtBuf[260];
        uint32_t evtLen = sizeof(evtBuf);
        
        // 创建 IOMemoryDescriptor 来包装我们的接收缓冲区
        md = IOMemoryDescriptor::withAddress(evtBuf, evtLen, kIODirectionIn);
        if (!md) {
            IOLog("tommybt: failed to create input memory descriptor\n");
            break;
        }
        
        uint32_t bytesRead = 0; // 这个变量非常重要，它会告诉我们实际读取了多少字节
        ret = _intInPipe->io(md, evtLen, bytesRead, 5000); // 5000ms timeout
        md->release(); // 释放 IOMemoryDescriptor

        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: intIn io failed, ret=0x%x\n", ret);
            break;
        }

        // 校验是不是 Command Complete
        if (bytesRead > 0 && evtBuf[0] == 0x0E) {
            if (bytesRead >= 6) {
                uint16_t rspOpcode = evtBuf[4] | (evtBuf[5] << 8);
                if (rspOpcode != opcode) {
                    IOLog("tommybt: warning: opcode mismatch 0x%04x vs 0x%04x\n", rspOpcode, opcode);
                }
            }
        } else if (bytesRead > 0) {
            IOLog("tommybt: got non-CC event 0x%02x\n", evtBuf[0]);
        } else {
            IOLog("tommybt: intIn read returned 0 bytes\n");
        }

        offset += 3 + plen;
    }

    fwData->release();
    IOLog("tommybt: firmware upload done (loop finished)\n");
    
    return (offset >= len);
}
 */

/*
bool QCABluetooth::uploadFirmware----old() {
    IOLog("tommybt: uploadFirmware start\n");

    // 1. 从内存加载固件
    OSData* fwData = OSData::withBytes(qca_firmware_hcd, qca_firmware_hcd_len);
    if (!fwData) {
        IOLog("tommybt: firmware could not be loaded into OSData\n");
        return false;
    }

    const uint8_t* data = (const uint8_t*)fwData->getBytesNoCopy();
    size_t len = fwData->getLength();

    IOLog("tommybt: firmware size = %lu\n", len);

    // 2. 按照 HCI HCD 格式逐条发送
    size_t offset = 0;
    while (offset < len) {
        // HCD 文件通常： [opcode(2) length(1) payload(N)]
        if (offset + 3 > len) break;

        uint16_t opcode = data[offset] | (data[offset+1] << 8);
        uint8_t plen = data[offset+2];

        if (offset + 3 + plen > len) break;

        const uint8_t* payload = &data[offset+3];

        // 构造 HCI 命令包
        // [0x01 | opcode(2) | length(1) | payload(N)]
        uint8_t buf[260];
        buf[0] = 0x01; // HCI Command packet
        buf[1] = data[offset];     // opcode low
        buf[2] = data[offset+1];   // opcode high
        buf[3] = plen;
        memcpy(&buf[4], payload, plen);

        size_t cmdLen = 4 + plen;

        // 3. 发送到 bulk out (使用新API)
        IOReturn ret = _bulkOutPipe->WritePipeSync(buf, cmdLen, 0, 5000); // 5000ms timeout
        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: WritePipeSync failed ret=0x%x\n", ret);
            fwData->release();
            return false;
        }

        // 4. 阻塞等待 Command Complete (使用新API)
        uint8_t evtBuf[260];
        size_t evtLen = sizeof(evtBuf);
        ret = _intInPipe->ReadPipeSync(evtBuf, &evtLen, 0, 5000); // 5000ms timeout
        if (ret != kIOReturnSuccess) {
            IOLog("tommybt: ReadPipeSync failed ret=0x%x\n", ret);
            fwData->release();
            return false;
        }

        // 校验是不是 Command Complete
        if (evtLen > 0 && evtBuf[0] == 0x0E) { // Event = Command Complete
            if (evtLen >= 6) {
                uint16_t rspOpcode = evtBuf[4] | (evtBuf[5] << 8);
                if (rspOpcode != opcode) {
                    IOLog("tommybt: warning: opcode mismatch 0x%04x vs 0x%04x\n", rspOpcode, opcode);
                }
            }
        } else if (evtLen > 0) {
            IOLog("tommybt: got non-CC event 0x%02x\n", evtBuf[0]);
        } else {
            IOLog("tommybt: intIn read returned 0 bytes\n");
        }

        offset += 3 + plen;
    }

    fwData->release();
    IOLog("tommybt: firmware upload done\n");
    return true;
}
 */

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
            uint8_t numPackets = data[2]; // 可并行命令数量
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
            uint8_t numPackets = data[3];
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

