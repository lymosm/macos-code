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
    memset(&input, 0, sizeof(input));
    strncpy(input.device, "EC0", sizeof(input.device)-1);
    strncpy(input.method, "_Q13", sizeof(input.method)-1);

    IOReturn ret = IOConnectCallStructMethod(
        connect,
        kEvaluateAcpiMethod,   // 注意不要再用 AsusKeysUserClient:: 前缀，直接用共享头文件里定义的枚举
        &input, sizeof(input),
        nullptr, nullptr);

    printf("IOConnectCallStructMethod ret = 0x%x\n", ret);

    IOServiceClose(connect);
    IOObjectRelease(service);
    return 0;
}

