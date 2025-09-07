//
//  AsusHIDHotkey.cpp
//
//

#include "AsusHIDHotkey.hpp"
#include "AsusFn.hpp"

#define super IOService
OSDefineMetaClassAndStructors(AsusHIDHotkey, IOService)

bool AsusHIDHotkey::start(IOService* provider) {
    IOHIKeyboard* keyboard = OSDynamicCast(IOHIKeyboard, provider);
    if (!keyboard) {
        IOLog("AsusHIDHotkey::start - not a keyboard\n");
        return false;
    }

    IOLog("AsusHIDHotkey::start - keyboard found, registering event handler\n");
    keyboard->registerEventAction(&AsusHIDHotkey::keyboardEventCallback, this);

    return super::start(provider);
}

void AsusHIDHotkey::stop(IOService* provider) {
    IOLog("AsusHIDHotkey::stop\n");
    super::stop(provider);
}

void AsusHIDHotkey::keyboardEventCallback(OSObject* target,
                                          unsigned eventType,
                                          unsigned flags,
                                          unsigned key,
                                          unsigned charCode,
                                          unsigned charSet,
                                          unsigned origCharCode,
                                          unsigned origCharSet,
                                          unsigned keyboardType,
                                          bool repeat,
                                          AbsoluteTime ts) {
    if (eventType == NX_KEYDOWN) {
        auto* self = OSDynamicCast(AsusHIDHotkey, target);
        if (self) {
            self->handleKeyPress(key);
        }
    }
}

void AsusHIDHotkey::handleKeyPress(unsigned key) {
    IOLog("AsusHIDHotkey::handleKeyPress - key: 0x%02X\n", key);

    if (key == 0x91) {
        IOLog("AsusHIDHotkey - Brightness Up triggered via HID\n");
        auto smc = AsusFn::getSharedInstance();
        if (smc) {å
            smc->increaseBrightness();
        }
    }
}
