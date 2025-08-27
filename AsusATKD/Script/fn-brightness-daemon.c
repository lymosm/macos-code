#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>

#define ASUS_VID 0x0b05
#define ASUS_PID 0x1854

// HID 输入报告回调
static void handle_input(void* context,
                         IOReturn result,
                         void* sender,
                         IOHIDValueRef value) {
    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (!elem) return;

    uint32_t scancode = IOHIDElementGetUsage(elem);
    CFIndex pressed = IOHIDValueGetIntegerValue(value);

    printf("tommydebug: usage=0x%x pressed=%ld\n", scancode, (long)pressed);

    // TODO: 在这里根据 Fn 键 usage 调用亮度调节 / 背光调节函数
}

// 创建匹配字典
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

int main() {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        printf("tommydebug: failed to create IOHIDManager\n");
        return -1;
    }

    // 匹配 Asus 键盘 (VID/PID)
    CFMutableDictionaryRef match = matching_dictionary(ASUS_VID, ASUS_PID);
    IOHIDManagerSetDeviceMatching(manager, match);
    if (match) CFRelease(match);

    // 打开 HID Manager
    IOReturn ret = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (ret != kIOReturnSuccess) {
        printf("tommydebug: IOHIDManagerOpen failed, ret=0x%x\n", ret);
        CFRelease(manager);
        return -1;
    }

    // 注册输入值回调
    IOHIDManagerRegisterInputValueCallback(manager, handle_input, NULL);

    // 获取 RunLoop
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    printf("tommydebug: daemon started, waiting for HID events...\n");
    CFRunLoopRun();

    CFRelease(manager);
    return 0;
}
