#include "smc.h"

// =============================================================================
//  START: Exact C translation of structures from the provided smc.h
//  这是成功的关键。这些结构体的布局和大小现在与工作的 smc tool 完全一致。
// =============================================================================

// Corresponds to SMCKeyData_vers_t
typedef struct {
    char     major;
    char     minor;
    char     build;
    char     reserved[1];
    uint16_t release;
} C_SMCKeyData_vers_t;

// Corresponds to SMCKeyData_pLimitData_t
typedef struct {
    uint16_t version;
    uint16_t length;
    uint32_t cpuPLimit;
    uint32_t gpuPLimit;
    uint32_t memPLimit;
} C_SMCKeyData_pLimitData_t;

// Corresponds to SMCKeyData_keyInfo_t
typedef struct {
    uint32_t dataSize;
    uint32_t dataType;
    char     dataAttributes;
} C_SMCKeyData_keyInfo_t;

// The one, true SMCKeyData_t structure
typedef struct {
    uint32_t                    key;
    C_SMCKeyData_vers_t         vers;
    C_SMCKeyData_pLimitData_t   pLimitData;
    C_SMCKeyData_keyInfo_t      keyInfo;
    char                        result;
    char                        status;
    char                        data8;
    uint32_t                    data32;
    char                        bytes[32];
} C_SMCKeyData_t;

// =============================================================================
//  END: Structure definitions
// =============================================================================

#define KERNEL_INDEX_SMC     2
#define SMC_CMD_READ_BYTES   5
#define SMC_CMD_READ_KEYINFO 9

static io_connect_t kIOConnection = 0;

// --- Utility Functions ---
uint32_t str_to_key(const char *str) {
    return (uint32_t)str[0] << 24 | (uint32_t)str[1] << 16 | (uint32_t)str[2] << 8 | (uint32_t)str[3];
}

void key_to_str(uint32_t val, char *str) {
    snprintf(str, 5, "%c%c%c%c", (val >> 24) & 0xFF, (val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
}

float fpe2_to_float(const char* bytes) {
    uint16_t raw_value = ((uint16_t)(unsigned char)bytes[0] << 8) | (uint16_t)(unsigned char)bytes[1];
    return (float)raw_value / 4.0f;
}

// --- IOKit Communication ---
static kern_return_t SMCCall(C_SMCKeyData_t *input, C_SMCKeyData_t *output) {
    size_t structureSize = sizeof(C_SMCKeyData_t);
    return IOConnectCallStructMethod(kIOConnection, KERNEL_INDEX_SMC, input, structureSize, output, &structureSize);
}

bool SMCOpen(void) {
    if (kIOConnection) return true;
    CFMutableDictionaryRef matchingDict = IOServiceMatching("AppleSMC");
    io_iterator_t iterator;
    kern_return_t result = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator);
    if (result != kIOReturnSuccess){
        printf("IOServiceGetMatchingServices failed\n");
        return false;
    }
    printf("IOServiceGetMatchingServices success\n");
    io_object_t device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0){
        printf("IOIteratorNext failed\n");
        return false;
    }
    printf("IOIteratorNext success\n");
    
    io_name_t name;
    kern_return_t kr = IORegistryEntryGetName(device, name);
    if (kr == KERN_SUCCESS) {
        printf("IOService device name: %s\n", name);
    } else {
        printf("IORegistryEntryGetName failed: 0x%x\n", kr);
    }
    
    result = IOServiceOpen(device, mach_task_self(), 0, &kIOConnection);
    IOObjectRelease(device);
    printf("IOServiceOpen result = 0x%x (%d)\n", result, result);

    // return true;
    if(result != kIOReturnSuccess){
        printf("IOServiceOpen failed\n");

        return false;
    }
    printf("IOServiceOpen success\n");
    return result == kIOReturnSuccess;
}

void SMCClose(void) {
    if (kIOConnection) {
        IOServiceClose(kIOConnection);
        kIOConnection = 0;
    }
}

// --- Core Logic ---
bool SMCReadKey(const char* key, C_SMCVal_t *val) {
    if (!key || !val || !SMCOpen()) return false;

    C_SMCKeyData_t input, output;
    kern_return_t result;

    // Step 1: Read Key Info
    memset(&input, 0, sizeof(C_SMCKeyData_t));
    input.key = str_to_key(key);
    input.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall(&input, &output);
    if (result != kIOReturnSuccess) {
        return false;
    }
    
    // Step 2: Use Key Info to Read Value
    val->dataSize = output.keyInfo.dataSize;
    key_to_str(output.keyInfo.dataType, val->dataType);
    strncpy(val->key, key, 5);

    memset(&input, 0, sizeof(C_SMCKeyData_t));
    input.key = str_to_key(key);
    input.keyInfo.dataSize = val->dataSize;
    input.data8 = SMC_CMD_READ_BYTES;

    result = SMCCall(&input, &output);
    if (result != kIOReturnSuccess) {
        return false;
    }

    memcpy(val->bytes, output.bytes, sizeof(output.bytes));
    return true;
}

int SMCGetFanRPM(int fanIndex){
    char fan_key[5];
    snprintf(fan_key, 5, "F%dAc", fanIndex);
    int rpm = 0;
    C_SMCVal_t fan_val;
    if (SMCReadKey(fan_key, &fan_val)) {
        if (strcmp(fan_val.dataType, "fpe2") == 0 && fan_val.dataSize >= 2) {
            rpm = (int)fpe2_to_float(fan_val.bytes);
            printf("  - Fan Speed (%s): %d RPM\n", fan_key, rpm);
        }
    }
    return rpm;
}
