#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <string.h>
#include "AsusKeysShared.h"   // 用公共头，而不是 AsusKeysUserClient.hpp

int main() {
    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("AsusKeys")); // IOClass from Info.plist

    if (!service) {
        printf("Service not found\n");
        return -1;
    }

    io_connect_t connect;
    if (IOServiceOpen(service, mach_task_self(), 0, &connect) != KERN_SUCCESS) {
        printf("Failed to open user client\n");
        IOObjectRelease(service);
        return -1;
    }

    AcpiMethodInput input;
    AcpiMethodOutput output = {};
    size_t outSize = sizeof(output);
    memset(&input, 0, sizeof(input));
    strncpy(input.device, "EC0", sizeof(input.device)-1);
    strncpy(input.method, "_Q13", sizeof(input.method)-1);
    input.argCount = 1;
    input.args[0].type = ACPI_ARG_TYPE_INT;
    input.args[0].intValue = 3;
    // input.args[1] = 456;

    IOReturn ret = IOConnectCallStructMethod(connect,
                                             kEvaluateAcpiMethod,
                                             &input, sizeof(input),
                                             &output, &outSize);

    if (ret == kIOReturnSuccess) {
        if (output.type == ACPI_RET_INT) {
            printf("ACPI returned int: %llu\n", output.intValue);
        } else if (output.type == ACPI_RET_STRING) {
            printf("ACPI returned string: %s\n", output.strValue);
        } else {
            printf("ACPI returned no value\n");
        }
    }
    printf("IOConnectCallStructMethod ret = 0x%x\n", ret);

    IOServiceClose(connect);
    IOObjectRelease(service);
    return 0;
}

