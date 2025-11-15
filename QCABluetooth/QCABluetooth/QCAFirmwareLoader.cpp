#include "QCAFirmwareLoader.hpp"
#include "qca_firmware.h"   // 包含 qca_mainfw / qca_rampatch / 长度变量
#include "QCABluetooth.hpp"

OSDefineMetaClassAndStructors(QCAFirmwareLoader, OSObject);

bool QCAFirmwareLoader::initWithInterface(IOUSBHostInterface* iface,
                                          IOUSBHostPipe* bulkIn,
                                          IOUSBHostPipe* bulkOut,
                                          IOUSBHostPipe* intIn,
                                          IOUSBHostDevice* device,
                                          QCABluetooth* owner,
                                          uint16_t bulkOutMaxPacket) {
    if (!iface || !bulkOut) return false;

    _iface = iface;
    _iface->retain();

    _bulkIn = bulkIn;
    if (_bulkIn) _bulkIn->retain();

    _bulkOutPipe = bulkOut;
    if (_bulkOutPipe) _bulkOutPipe->retain();

    _intIn = intIn;
    if (_intIn) _intIn->retain();

    _device = device;
    if (_device) _device->retain();

    _bulkOutMaxPacket = bulkOutMaxPacket;

    // 保存并 retain owner（QCABluetooth），用于 later close()
    _owner = owner;
    if (_owner) _owner->retain();

    return true;
}

bool QCAFirmwareLoader::uploadFirmware() {
    IOLog("tommybt: uploadFirmware start\n");

    // 上传 main firmware
    IOLog("tommybt: uploading main firmware, size=%lu\n", qca_mainfw_len);
    if (!uploadFirmwareFile_ControlThenBulk(qca_mainfw, qca_mainfw_len)) {
        IOLog("tommybt: main firmware upload failed\n");
        return false;
    }

    // 上传 rampatch
    IOLog("tommybt: uploading rampatch, size=%lu\n", qca_rampatch_len);
    if (!uploadFirmwareFile_ControlThenBulk(qca_rampatch, qca_rampatch_len)) {
        IOLog("tommybt: rampatch upload failed\n");
        return false;
    }

    IOLog("tommybt: firmware upload done\n");
    return true;
}

bool QCAFirmwareLoader::uploadFirmwareFile_ControlThenBulk(const uint8_t* data, size_t len) {
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
    IOReturn r = _device->deviceRequest(_owner, ctlreq, (void*)data, bytesTransferred, CTRL_TIMEOUT);
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

bool QCAFirmwareLoader::sendHciReset() {
    IOLog("tommybt: sendHciReset in\n");

    // HCI Reset 命令 (OGF=0x03, OCF=0x0003, 参数长度=0)
    uint8_t cmd[4] = {0x01, 0x03, 0x0C, 0x00};

    if (!_device || !_owner || !_iface) {
        IOLog("tommybt: sendHciReset missing device/owner/iface\n");
        return false;
    }

    // 获取 interface number（关键）
    const InterfaceDescriptor *ifDesc = _iface->getInterfaceDescriptor();
    uint16_t ifNum = ifDesc ? ifDesc->bInterfaceNumber : 0;

    StandardUSB::DeviceRequest req;
    bzero(&req, sizeof(req));
    req.bmRequestType = makeDeviceRequestbmRequestType(
        kRequestDirectionOut,       // Host -> Device
        kRequestTypeClass,          // Class-specific
        kRequestRecipientInterface  // 针对 interface
    );
    req.bRequest = 0;   // HCI Class command 使用 bRequest=0
    req.wValue   = 0;
    req.wIndex   = ifNum;       // interface number
    req.wLength  = sizeof(cmd);

    uint32_t bytesTransferred = 0;
    IOLog("tommybt: HCI Reset sent before deviceRequest\n");
    IOReturn err = _device->deviceRequest(_owner, req, (void*)cmd, bytesTransferred, 5000);
    if (err != kIOReturnSuccess) {
        IOLog("tommybt: HCI Reset deviceRequest failed 0x%x (if=%u)\n", err, ifNum);
        return false;
    }

    IOLog("tommybt: HCI Reset sent via control, bytes=%u\n", bytesTransferred);
    IOSleep(200); // 给固件应用时间
    return true;
}


void QCAFirmwareLoader::detachAndRelease() {
    IOLog("tommybt: detaching interface, letting IOBluetooth probe\n");

    // 使用之前保存的 owner 来 close 接口（必须用 open 时相同的 client pointer）
    if (_iface) {
        if (_owner) {
            // 如果 iface 对 owner 是 open 状态，则用 owner close
            // （isOpen 可选：某些 SDK 支持）
            bool isOpenByOwner = false;
            // 尝试调用 isOpen，如果不存在则跳过检测（用 try/catch 无效，编译期要保证符号存在）
            // 大多数 IOService 子类都有 isOpen(OSObject*)
            // 我这里做简单地直接调用 close(_owner)
            _iface->close(_owner);
            IOLog("tommybt: closed interface with owner %p\n", _owner);
        } else {
            IOLog("tommybt: owner is NULL, cannot close iface with original client pointer\n");
            // 不要用 'this' 去 close（因为 open 是由 QCABluetooth 用 this 打开的）
        }

        // release iface object
        _iface->release();
        _iface = nullptr;
    }

    if (_bulkIn)  { _bulkIn->release();  _bulkIn = nullptr; }
    if (_bulkOutPipe) { _bulkOutPipe->release(); _bulkOutPipe = nullptr; }
    if (_intIn)   { _intIn->release();   _intIn = nullptr; }

    if (_device)  { _device->release();  _device = nullptr; }
}

// 覆盖 free() 确保 owner 被 release，并清理残留
void QCAFirmwareLoader::free() {
    IOLog("tommybt: QCAFirmwareLoader free\n");
    // detachAndRelease 已经能安全处理多次调用（_iface/_pipes 已检查 nullptr）
    detachAndRelease();

    if (_owner) {
        _owner->release();
        _owner = nullptr;
    }

    OSObject::free();
}

