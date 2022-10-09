/* add your code here */
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

class HelloWorld : public IOService{
    OSDeclareDefaultStructors(HelloWorld);
public:
    virtual bool init(OSDictionary* dict) override;
    
    virtual void free(void) override;
    
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    
    virtual bool start(IOService* provider) override;
    
    virtual void stop(IOService* provider) override;
};
