#ifndef TimeSync_hpp
#define TimeSync_hpp

#include <IOKit/IOService.h>

class TimeSync : public IOService {
    OSDeclareDefaultStructors(TimeSync)

public:
    virtual bool init(OSDictionary* dict = nullptr) override;
    virtual void free() override;

    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
};
#endif /* TimeSync_hpp */
