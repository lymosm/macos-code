#include <stdio.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <usage> <keycode>\n", argv[0]);
        return 1;
    }

    uint32_t usage   = (uint32_t)strtoul(argv[1], NULL, 0);
    uint32_t keycode = (uint32_t)strtoul(argv[2], NULL, 0);
    uint32_t usagePage = 0xff31;
    uint32_t pressed = 1;

    io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("AsusKeys"));
    if (!svc) {
        printf("tommydebug: cannot find AsusKeys service\n");
        return 1;
    }

    io_connect_t conn = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
    IOObjectRelease(svc);

    if (kr != KERN_SUCCESS) {
        printf("tommydebug: IOServiceOpen failed (0x%x)\n", kr);
        return 1;
    }

    uint64_t input[4] = { usagePage, usage, pressed, keycode};


    kr = IOConnectCallMethod(conn,
                             0,            // selector = 0 (AsusKeys 接收 usage/keycode)
                             input, 4,     // input scalars
                             NULL, 0,      // no struct input
                             NULL, NULL,
                             NULL, NULL);  // no struct output

    if (kr != KERN_SUCCESS) {
        printf("tommydebug: IOConnectCallMethod failed (0x%x)\n", kr);
    } else {
        printf("tommydebug: sent usage=0x%x keycode=0x%x\n", usage, keycode);
    }

    IOServiceClose(conn);
    return 0;
}

