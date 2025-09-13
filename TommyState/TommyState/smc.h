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
// --- End of smc.h content ---
