#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

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
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // Minimal helpers
    bool findAndOpenInterface();
    bool uploadFirmware();
    void handleUSBData();
};
