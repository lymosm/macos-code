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



#define ASUS_VID 0x0b05
#define ASUS_PID 0x1854

static int force_keymap = 0;  // 是否强制键盘映射

// --- DisplayServices 私有 API ---
typedef int (*DisplayServicesSetBrightnessPtr)(uint32_t display, float brightness);
static DisplayServicesSetBrightnessPtr pSet = NULL;

static void init_display_services() {
    void *handle = dlopen("/System/Library/PrivateFrameworks/DisplayServices.framework/DisplayServices", RTLD_LAZY);
    if (!handle) {
        printf("tommydebug: cannot open DisplayServices.framework\n");
        return;
    }
    pSet = (DisplayServicesSetBrightnessPtr)dlsym(handle, "DisplayServicesSetBrightness");
    if (!pSet) {
        printf("tommydebug: cannot resolve DisplayServicesSetBrightness\n");
    } else {
        printf("tommydebug: DisplayServices ready\n");
    }
}

// --- IODisplay 调整亮度 ---
static io_service_t get_display_service() {
    io_iterator_t it;
    io_service_t service = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                     IOServiceMatching("IODisplayConnect"),
                                     &it) == KERN_SUCCESS) {
        service = IOIteratorNext(it);
        IOObjectRelease(it);
    }
    return service;
}

static int adjust_brightness(float delta) {
    if (force_keymap) {
       // printf("tommydebug: force_keymap enabled, skipping DisplayServices\n");
        return -1;
    }

    io_service_t display = get_display_service();
    if (!display) {
        printf("tommydebug: no IODisplay service found\n");
        return -1;
    }

    static float brightness = 0.5f;
    brightness += delta;
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;

    IOReturn ret = IODisplaySetFloatParameter(display, kNilOptions,
                                              CFSTR(kIODisplayBrightnessKey), brightness);
    IOObjectRelease(display);

   // printf("tommydebug: [IODisplay] set brightness=%.2f (ret=0x%x)\n", brightness, ret);

    if (pSet) {
        int sret = pSet(0, brightness);
       // printf("tommydebug: [DisplayServices] set brightness=%.2f (ret=%d)\n", brightness, sret);
        return sret;
    }

    return -1; // 不支持 DisplayServices
}

// --- 模拟键盘事件 ---
static void send_key(uint16_t keycode) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, false);
    if (down && up) {
        CGEventPost(kCGHIDEventTap, down);
        CGEventPost(kCGHIDEventTap, up);
       // printf("tommydebug: [Fallback] sent keycode=0x%x\n", keycode);
    }
    if (down) CFRelease(down);
    if (up) CFRelease(up);
}


static void sleep_system() {
   // printf("tommydebug: Fn+F1 pressed, sleeping...\n");
        // 调用系统命令触发睡眠
        system("pmset sleepnow");
}

// --- HID 回调 ---
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
       // printf("tommydebug: usage=0x%x page=0x%x pressed=%ld\n", usage, page, (long)pressed);

        // 只处理 Asus Fn 键 (page=0xFF31)
        if (page == 0xff31) {
            if (usage == 0x10) {        // Fn+F5 亮度减
                if (adjust_brightness(-0.1f) != 0) send_key(0x6B);
            } else if (usage == 0x20) { // Fn+F6 亮度加
                if (adjust_brightness(+0.1f) != 0) send_key(0x71);
            } else if (usage == 0xC5) { // F3 背光减
                send_key(0x6F); // 对应原生背光减
               // printf("tommydebug: F3 pressed, reduce keyboard backlight\n");
            } else if (usage == 0xC4) { // F4 背光加
                send_key(0x70); // 对应原生背光加
               // printf("tommydebug: F4 pressed, increase keyboard backlight\n");
            }
            
            if (usage == 0x6c && page == 0xff31) { // Fn+F1
               // printf("tommydebug: Fn+F1 pressed, sleeping...\n");
                sleep_system();
            }
        }
    }
}

// 匹配字典
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
    // 检查参数
    if (argc > 1 && strcmp(argv[1], "--force-keymap") == 0) {
        force_keymap = 1;
        printf("tommydebug: running in force_keymap mode\n");
    }

    init_display_services();

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
