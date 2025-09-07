#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <ApplicationServices/ApplicationServices.h>  // CGEvent
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include "osd_bridge.h"


// 你现有的代码保持不变...

// AsusKeys.kext 服务定义
#define ASUS_VID 0x0b05
#define ASUS_PID 0x1854

// IOService连接和发送数据函数
static io_connect_t conn = IO_OBJECT_NULL;


static void send_key_info_to_kernel(uint32_t usage, uint32_t page, uint32_t pressed) {
    if (conn == IO_OBJECT_NULL) {
        io_service_t svc = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("AsusKeys"));
        if (svc) {
            kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
            IOObjectRelease(svc);
            if (kr != KERN_SUCCESS) {
                printf("tommydebug: IOServiceOpen failed (0x%x)\n", kr);
                return;
            }
        } else {
            printf("tommydebug: Cannot find AsusKeys service\n");
            return;
        }
    }

    // 发送 usage 和 keycode 到内核扩展
    uint64_t input[4] = { usage, page, pressed, 1 }; // 使用0xFF31为默认的 usagePage（例如 Asus 键盘的 HID Page）

    kern_return_t kr = IOConnectCallMethod(conn,
                                           0,           // selector (0 对应于你在内核端定义的 sLogUsage 方法)
                                           input, 4,    // input scalars
                                           NULL, 0,     // no struct input
                                           NULL, NULL,
                                           NULL, NULL); // no struct output

    if (kr != KERN_SUCCESS) {
        printf("tommydebug: IOConnectCallMethod failed (0x%x)\n", kr);
    } else {
        printf("tommydebug: Sent usage=0x%x, page=0x%x to kernel\n", usage, page);
    }
}

// --- HID 回调 ---
static void handle_input(void* context,
                         IOReturn result,
                         void* sender,
                         IOHIDValueRef value) {
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (!elem) return;

    uint32_t usage = IOHIDElementGetUsage(elem);
    uint32_t page  = IOHIDElementGetUsagePage(elem);
    CFIndex pressed = IOHIDValueGetIntegerValue(value);

    if (pressed) {
        if(page != 0xff31){
            return ;
        }
        // 输出按键信息
        printf("tommydebug: usage=0x%x page=0x%x pressed=%ld\n", usage, page, (long)pressed);
        
        if(usage == 0x6c){
            osd_gotoSleep();
        }
        // 向内核发送按键信息
        send_key_info_to_kernel(usage, page, pressed); // page 可根据需要替换为其他信息，比如 keycode
    }
}

// --- 匹配字典 ---
static CFMutableDictionaryRef matching_dictionary(int vendor, int product) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    if (!dict) return NULL;

    CFNumberRef vid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor);
    CFNumberRef pid = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &product);

    if (vid && pid) {
        CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vid);
        CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), pid);
    }

    if (vid) CFRelease(vid);
    if (pid) CFRelease(pid);

    return dict;
}

int main(int argc, char *argv[]) {

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        printf("tommydebug: failed to create IOHIDManager\n");
        return -1;
    }

    CFMutableDictionaryRef match = matching_dictionary(ASUS_VID, ASUS_PID);
    IOHIDManagerSetDeviceMatching(manager, match);
    if (match) CFRelease(match);

    IOReturn ret = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (ret != kIOReturnSuccess) {
        printf("tommydebug: IOHIDManagerOpen failed, ret=0x%x\n", ret);
        CFRelease(manager);
        return -1;
    }

    IOHIDManagerRegisterInputValueCallback(manager, handle_input, NULL);
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    printf("tommydebug: daemon started, waiting for HID events...\n");
    CFRunLoopRun();

    CFRelease(manager);
    return 0;
}
