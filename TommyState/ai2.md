以下是h文件：
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h> // For standard integer types like uint32_t
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>

// --- Start of smc.h content ---

// This structure holds the final value read from the SMC
// Corresponds to SMCVal_t
typedef struct {
    char     key[5];
    uint32_t dataSize;
    char     dataType[5];
    char     bytes[32];
} C_SMCVal_t;

bool SMCOpen(void);
void SMCClose(void);
float fpe2_to_float(const char* bytes);
bool SMCReadKey(const char *key, C_SMCVal_t *val);
int  SMCGetFanRPM(int fanIndex);
int  SMCListKeys(void);
// --- End of smc.h content --

以下是c文件：
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
    if (result != kIOReturnSuccess) return false;
    io_object_t device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) return false;
    result = IOServiceOpen(device, mach_task_self(), 0, &kIOConnection);
    IOObjectRelease(device);
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


以下是swift桥接文件：
import Foundation

final class SMCWrapper {
    static let shared = SMCWrapper()

    private var opened = false

    private init() {
        // attempt open now — safe no-op if fails
        let r = SMCOpen()
        if r == true {
            opened = true
            print("[SMCWrapper] SMCOpen succeeded")
        } else {
            opened = false
            print("[SMCWrapper] SMCOpen failed with code \(r)")
        }
    }

    deinit {
        if opened {
            SMCClose()
        }
    }

    /// Ensure connection is open; attempt to open if not yet.
    @discardableResult
    func ensureOpen() -> Bool {
        if opened { return true }
        let r = SMCOpen()
        if r == true {
            opened = true
            print("[SMCWrapper] SMCOpen succeeded on ensureOpen")
            return true
        } else {
            print("[SMCWrapper] SMCOpen failed in ensureOpen: \(r)")
            return false
        }
    }

    /// High-level: get fan RPM for given index (0-based). Returns nil on failure.
    func getFanRPM(index: Int) -> Int? {
        guard ensureOpen() else { return nil }
        let r = SMCGetFanRPM(Int32(index))
        if r >= 0 {
            return Int(r)
        }
        return nil
    }

    /// Generic: read arbitrary SMC key into Data
    func readKey(_ key: String) -> Data? {
        /*
        guard ensureOpen() else { return nil }
        var cKey = [CChar](repeating: 0, count: 5)
        let bytes = Array(key.utf8.prefix(4))
        for i in 0..<bytes.count {
            cKey[i] = CChar(bytes[i])
        }
        cKey[4] = 0
        var size: UInt32 = 128
        var buf = [UInt8](repeating: 0, count: Int(size))
        // let result = SMCReadKey(cKey, &buf, &size)
        let result = 0;
        if result == 0 {
            return Data(buf.prefix(Int(size)))
        } else {
            print("[SMCWrapper] SMCReadKey(\(key)) failed: \(result)")
            return nil
        }
         */
        return nil;
    }
}

桥接文件：
//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//

// TommyState-Bridging-Header.h
#import "smc.h"

可以读取风扇转速了，我集成到了swift项目下。使用桥接的方式。编译build报错：7 duplicate symbols
Linker command failed with exit code 1 (use -v to see invocation)
帮我看看什么原因？
