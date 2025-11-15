#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/usb/StandardUSB.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/usb/IOUSBHostFamily.h> // IOUSBHostDevice / IOUSBHostInterface / IOUSBHostPipe
#include <IOKit/usb/IOUSBHostPipe.h>
#include <IOKit/IORegistryEntry.h> // for gIOServicePlane
#include <IOKit/IOMemoryDescriptor.h> // 必须包含这个头文件


// 声明外部 class symbols（必要！）
extern const OSSymbol* gIOUSBHostInterfaceClass;
extern const OSSymbol* gIOUSBHostPipeClass;

// 如果 kUSBTransferTypeMask 未定义，手动补上：
#ifndef kUSBTransferTypeMask
#define kUSBTransferTypeMask 0x03
#endif

class QCABluetooth : public IOService {
    OSDeclareDefaultStructors(QCABluetooth)

private:
    IOUSBHostDevice *   _device;
    IOUSBHostInterface * _interface;
    IOWorkLoop *         _workloop;
    IOCommandGate *      _cmdGate;

public:
    virtual bool init(OSDictionary* dict) override;
    virtual void free() override;
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual IOReturn message(UInt32 type, IOService *provider, void *argument) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    

    // Minimal helpers
    bool findAndOpenInterface();
    bool uploadFirmware();
    bool uploadFirmwareFile(const uint8_t* data, size_t len);
    bool uploadFirmwareFile_ControlThenBulk(const uint8_t* data, size_t len);
    void handleUSBData(uint8_t* data, size_t len);
    
    IOUSBHostPipe* _intInPipe;                   // 事件输入 pipe
    IOBufferMemoryDescriptor* _eventBuffer;      // 存放事件数据


    IOUSBHostPipe* _bulkOutPipe;   // 发 HCI 命令
    IOUSBHostPipe* _bulkInPipe;    // 接收 ACL (以后可用)
    // member vars
    IOUSBHostInterface* _usbInterface = nullptr;
    uint16_t _bulkOutMaxPacket = 0;

    bool submitAsyncRead();
    void allocateEventRead();
    void eventReadComplete(void* owner,
                           void* parameter,
                           IOReturn status,
                           uint32_t bytesTransferred);
    IOReturn getDeviceStatus(IOService* forClient, USBStatus *status);
    bool sendHCIResetAndWait();
    IOReturn sendSmallBulkTest();
    void firmwareUploadComplete();

};
