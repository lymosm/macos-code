在黑苹果开发高通蓝牙kext项目下有2个蓝牙固件：
ar3k/AthrBT_0x31010100.dfu
ar3k/ramps_0x31010100_40.dfu

以下是固件上传函数：
bool QCABluetooth::uploadFirmware() {
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

1. 帮我写一个shell脚本：用以上2个蓝牙固件，生成qca_firmware.h 包含qca_firmware_hcd, qca_firmware_hcd_len 2个变量
2. 另外我有个疑问：为何固件有2个文件，是不是uploadFirmware也要跟着修改？
