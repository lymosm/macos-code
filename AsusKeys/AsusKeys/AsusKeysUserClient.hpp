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
#include "AsusKeysShared.h"   // 用公共头，而不是 AsusKeysUserClient.hpp
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
    /*
    enum {
        ACPI_ARG_TYPE_INT = 0,
        ACPI_ARG_TYPE_STRING = 1
    };

    struct AcpiMethodArg {
        uint32_t type;   // 0=int, 1=string
        uint64_t intValue;
        char     strValue[32];  // 简单限制字符串最大32字节
    };
    
    struct AcpiMethodInput {
        char device[16];   // 设备名，比如 "EC0" 或 "ATKD"
        char method[16];   // 方法名，比如 "_Q13"
        uint32_t argCount; // 参数个数
        AcpiMethodArg args[4];  // 参数值（简单起见，支持整数）
    };
    enum {
        ACPI_RET_NONE   = 0,
        ACPI_RET_INT    = 1,
        ACPI_RET_STRING = 2,
        ACPI_RET_BUFFER = 3   // 如果以后要支持 raw buffer
    };
    
    // 输出结果
    typedef struct {
        uint32_t type;     // ACPI_RET_XXX
        uint64_t intValue; // 整数结果
        char     strValue[64];  // 字符串结果，简单限制 64 字节
    } AcpiMethodOutput;
    */
    
    static IOReturn sEvaluateAcpiMethod(OSObject* target, void* reference, IOExternalMethodArguments* args);
        IOReturn evaluateAcpiMethod(IOExternalMethodArguments* args);

    static const IOExternalMethodDispatch sMethods[kNumberOfMethods];
};

#endif /* AsusKeysUserClient_hpp */
