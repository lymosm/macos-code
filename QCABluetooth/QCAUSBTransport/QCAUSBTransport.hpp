#ifndef _QCAUSBTRANSPORT_HPP
#define _QCAUSBTRANSPORT_HPP

#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/bluetooth/transport/IOBluetoothHostControllerUSBTransport.h>

class QCAUSBTransport : public IOBluetoothHostControllerUSBTransport {
    OSDeclareDefaultStructors(QCAUSBTransport)

private:
    IOUSBHostInterface* fInterface = nullptr;
    IOUSBHostDevice*    fDevice = nullptr;

    IOUSBHostInterface* findReadyInterface(IOService* device, uint32_t maxWaitMs = 10000, uint32_t pollMs = 200);

public:
    virtual bool init(OSDictionary* dict = nullptr) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOReturn message(UInt32 type, IOService* provider, void* argument = nullptr) override;
};

#endif // _QCAUSBTRANSPORT_HPP

