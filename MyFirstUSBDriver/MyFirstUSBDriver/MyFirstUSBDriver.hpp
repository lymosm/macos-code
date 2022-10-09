/* add your code here */
#include <IOKit/usb/IOUSBHostDevice.h>

class lymosUSBDriver : public IOService{
    OSDeclareDefaultStructors(lymosUSBDriver)
public:
    virtual bool init(OSDictionary *propTable) override;
    virtual IOService* probe(IOService *provider, SInt32 *score) override;
    virtual bool attach(IOService *provider) override;
    virtual void detach(IOService *provider) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual bool terminate(IOOptionBits options = 0) override;
};
