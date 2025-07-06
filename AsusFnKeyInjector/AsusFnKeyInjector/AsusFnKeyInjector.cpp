#include "AsusFnKeyInjector.hpp"
#include <IOKit/IOLib.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AsusFnKeyInjector, IOHIKeyboard)

bool AsusFnKeyInjector::start(IOService* provider) {
    IOLog("AsusFnKeyInjector::start\n");

    if (!super::start(provider)) {
        IOLog("super::start failed\n");
        return false;
    }

    registerService();
    return true;
}

void AsusFnKeyInjector::stop(IOService* provider) {
    IOLog("AsusFnKeyInjector::stop\n");
    super::stop(provider);
}

// ❗不要 override，它不是 virtual 函数
void AsusFnKeyInjector::dispatchKeyboardEvent(unsigned eventType, unsigned flags, unsigned keyCode) {
    IOLog("AsusFnKeyInjector: key=%u flags=%u eventType=%u\n", keyCode, flags, eventType);

    if (keyCode == 0x91 /* 假设为 Fn+F6 */) {
        IOLog("Fn + F6 Detected! Trigger brightness++\n");
    }

    // 手动调用父类（注意：不是必须）
    super::dispatchKeyboardEvent(eventType, flags, keyCode);
}
