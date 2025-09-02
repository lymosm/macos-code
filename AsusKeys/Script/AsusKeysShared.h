//
//  AsusKeysShared.h
//  AsusKeys
//
//  Created by lymos on 2025/9/2.
//

#ifndef AsusKeysShared_h
#define AsusKeysShared_h

// 和 KEXT & 用户态共享的结构体
// method selector 枚举
enum {
    kLogUsage = 0,
    kEvaluateAcpiMethod,
    kNumberOfMethods
};

enum {
    ACPI_ARG_TYPE_INT = 0,
    ACPI_ARG_TYPE_STRING = 1
};
struct AcpiMethodArg {
    uint32_t type;   // 0=int, 1=string
    uint64_t intValue;
    char   strValue[32];  // 简单限制字符串最大32字节
};

typedef struct {
    char device[16];   // "EC0" / "ATKD"
    char method[16];   // "_Q13" ...
    uint32_t argCount; // 参数个数
    AcpiMethodArg args[4];  // 参数值（简单起见，支持整数）
    
} AcpiMethodInput;

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



#endif /* AsusKeysShared_h */
