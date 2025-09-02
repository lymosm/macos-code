//
//  AsusKeysUserClient.hpp
//  AsusKeys
//
//  Created by lymos on 2025/8/30.
//

// AsusKeysUserClient.hpp
#ifndef AsusKeysUserClient_hpp
#define AsusKeysUserClient_hpp

#include <IOKit/IOUserClient.h>
#include "AsusKeys.hpp"

#pragma pack(push,1)
typedef struct {
    uint32_t usagePage;
    uint32_t usage;
    int32_t  pressed;
    uint32_t keycode;
} AsusKeyMessage;
#pragma pack(pop)

class AsusKeysUserClient : public IOUserClient {
    OSDeclareDefaultStructors(AsusKeysUserClient)

private:
    task_t      fTask;
    AsusKeys*   fProvider;

public:
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOReturn clientClose(void) override;
    virtual IOReturn externalMethod(uint32_t selector,
                                    IOExternalMethodArguments* arguments,
                                    IOExternalMethodDispatch* dispatch,
                                    OSObject* target,
                                    void* reference) override;

    // dispatch entry
    static IOReturn sLogUsage(OSObject* target, void* reference, IOExternalMethodArguments* args);
    IOReturn logUsage(IOExternalMethodArguments* args);

    enum {
        kLogUsage = 0,
        kEvaluateAcpiMethod,
        kNumberOfMethods
        
    };
    
    struct AcpiMethodInput {
        char device[16];   // 设备名，比如 "EC0" 或 "ATKD"
        char method[16];   // 方法名，比如 "_Q13"
    };
    
    static IOReturn sEvaluateAcpiMethod(OSObject* target, void* reference, IOExternalMethodArguments* args);
        IOReturn evaluateAcpiMethod(IOExternalMethodArguments* args);

    static const IOExternalMethodDispatch sMethods[kNumberOfMethods];
};

#endif /* AsusKeysUserClient_hpp */
