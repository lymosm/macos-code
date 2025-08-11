#include "AsusFnKeyInjector.hpp"
#include <IOKit/IOLib.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AsusFnKeyInjector, IOHIKeyboard);

// 日志宏
#define LOG(fmt, ...) IOLog("AsusFnKeyInjector: " fmt "\n", ##__VA_ARGS__)

bool AsusFnKeyInjector::init(OSDictionary *dict) {
    if (!super::init(dict)) {
        return false;
    }
    acpiDevice = nullptr;
    maxBacklightLevel = 0;
    return true;
}

void AsusFnKeyInjector::free() {
    if (acpiDevice) {
        acpiDevice->release();
    }
    super::free();
}

bool AsusFnKeyInjector::start(IOService *provider) {
    if (!super::start(provider)) {
        LOG("Failed to start parent");
        return false;
    }
    
    // 查找 ACPI 设备
    acpiDevice = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (!acpiDevice) {
        LOG("Provider is not an ACPI device");
        return false;
    }
    acpiDevice->retain();

    
    LOG("Driver started (Max backlight: %u)", maxBacklightLevel);
    registerService();
    return true;
}

void AsusFnKeyInjector::stop(IOService *provider) {
    LOG("Driver stopping");
    super::stop(provider);
}

void AsusFnKeyInjector::dispatchKeyboardEvent(unsigned eventType,
                                            unsigned flags,
                                            unsigned keyCode) {
    // 处理 Fn+F4 (ASUS 笔记本通常使用 0x63 作为 Fn+F4 的键码)
    if (keyCode == 0x63 && (flags & NX_COMMANDMASK)) {
        adjustKeyboardBacklight(1); // 增加背光
        return;
    }
    
    super::dispatchKeyboardEvent(eventType, flags, keyCode);
}

#pragma mark - 背光控制


void AsusFnKeyInjector::setBacklightLevel(UInt32 level) {
    if (!acpiDevice || level > maxBacklightLevel) return;
    
    OSObject *params[1] = { OSNumber::withNumber(level, 32) };
    acpiDevice->evaluateObject("KBLT", nullptr, params, 1);
    OSSafeReleaseNULL(params[0]);
    
    LOG("Set backlight to %u/%u", level, maxBacklightLevel);
}

void AsusFnKeyInjector::adjustKeyboardBacklight(int delta) {
    UInt32 current = getCurrentBacklightLevel();
    UInt32 newLevel = current + delta;
    
    // 确保在有效范围内
    if (delta > 0 && newLevel > maxBacklightLevel) {
        newLevel = maxBacklightLevel;
    } else if (delta < 0 && newLevel > maxBacklightLevel) { // 处理下溢
        newLevel = 0;
    }
    
    setBacklightLevel(newLevel);
}

#pragma mark - 驱动入口

extern "C" {
    kern_return_t AsusFnKeyInjector_start(kmod_info_t *ki, void *d);
    kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d);
    
    kern_return_t AsusFnKeyInjector_start(kmod_info_t *ki, void *d) {
        LOG("Driver loaded");
        return KERN_SUCCESS;
    }
    
    kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d) {
        LOG("Driver unloaded");
        return KERN_SUCCESS;
    }
};
