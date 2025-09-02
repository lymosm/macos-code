//
//  AsusKeysShared.h
//  AsusKeys
//
//  Created by lymos on 2025/9/2.
//

#ifndef AsusKeysShared_h
#define AsusKeysShared_h

// 和 KEXT & 用户态共享的结构体
#pragma pack(push,1)
typedef struct {
    char device[16];   // "EC0" / "ATKD"
    char method[16];   // "_Q13" ...
} AcpiMethodInput;
#pragma pack(pop)

// method selector 枚举
enum {
    kLogUsage = 0,
    kEvaluateAcpiMethod,
    kNumberOfMethods
};

#endif /* AsusKeysShared_h */
