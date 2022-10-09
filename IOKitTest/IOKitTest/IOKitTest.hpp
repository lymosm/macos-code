/* add your code here */
#include <IOKit/IOService.h>
// #include <IOKit/IOLib.h>

class lymosIOKitTest : public IOService{
    OSDeclareDefaultStructors(lymosIOKitTest)
public:
    virtual bool init(OSDictionary* dictionary = NULL) override;
    virtual bool start(IOService* provider) override;
    
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    
    virtual void stop(IOService* provider) override;
    
    virtual void free(void) override;
};
