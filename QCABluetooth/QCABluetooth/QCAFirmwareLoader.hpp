#ifndef QCA_FIRMWARE_LOADER_HPP
#define QCA_FIRMWARE_LOADER_HPP

#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/usb/IOUSBHostPipe.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/StandardUSB.h>

class QCABluetooth; // 前向声明，避免循环包含

class QCAFirmwareLoader : public OSObject {
    OSDeclareDefaultStructors(QCAFirmwareLoader);

public:
    bool initWithInterface(IOUSBHostInterface* iface,
                           IOUSBHostPipe* bulkIn,
                           IOUSBHostPipe* bulkOut,
                           IOUSBHostPipe* intIn,
                           IOUSBHostDevice* device,
                           QCABluetooth* owner,
                           uint16_t bulkOutMaxPacket);

    bool uploadFirmware();
    bool sendHciReset();
    void detachAndRelease();

private:
    bool uploadFirmwareFile_ControlThenBulk(const uint8_t* data, size_t len);
    void free() override;

    IOUSBHostInterface* _iface {nullptr};
    IOUSBHostPipe* _bulkIn {nullptr};
    IOUSBHostPipe* _bulkOutPipe {nullptr};
    IOUSBHostPipe* _intIn {nullptr};
    IOUSBHostDevice* _device {nullptr};

    QCABluetooth* _owner {nullptr};    // 保存开 interface 时使用的 owner（QCABluetooth）
    uint16_t _bulkOutMaxPacket {0};
};

#endif // QCA_FIRMWARE_LOADER_HPP

