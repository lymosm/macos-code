#include "AsusSMC.hpp"
#include "AsusFnKeyListener.hpp"
// #include "AsusFnHIDHook.hpp"

bool ADDPR(debugEnabled) = false;
uint32_t ADDPR(debugPrintDelay) = 0;

// lymos
AsusSMC* AsusSMC::instance = nullptr;
AsusSMC* AsusSMC::getSharedInstance() {
    return instance;
}
void AsusSMC::increaseBrightness() {
    IOLog("AsusSMC::increaseBrightness called\n");

    // 实际实现可以写入 SMC 或 EC，比如：
    // writeSMCKey("LKSB", newValue); 或调用已有 brightness manager
}



#define super IOService
OSDefineMetaClassAndStructors(AsusSMC, IOService)


bool AsusSMC::start(IOService *provider) {
    if (!provider || !super::start(provider)) {
        SYSLOG("atk", "failed to start parent");
        return false;
    }
    SYSLOG("lymosatk", "start 88888888");
    atkDevice = (IOACPIPlatformDevice *)provider;

    parse_WDG();

    initATKDevice();

    initALSDevice();

    initEC0Device();

    initBattery();

    initVirtualKeyboard();

    startATKDevice();
    
    auto fnListener = new AsusFnKeyListener;
        if (fnListener && fnListener->init()) {
            fnListener->attach(this);
            fnListener->registerService();
            SYSLOG("lymoskey", "AsusSMC [FnKey] Fn key listener registered\n");
        } else {
            SYSLOG("lymoskey", "AsusSMC [FnKey] Failed to initialize Fn key listener\n");
        }

    // AsusFnHIDHook::install();
    
    workloop = getWorkLoop();
    if (!workloop) {
        DBGLOG("atk", "Failed to get workloop");
        return false;
    }
    workloop->retain();

    command_gate = IOCommandGate::commandGate(this);
    if (!command_gate || (workloop->addEventSource(command_gate) != kIOReturnSuccess)) {
        DBGLOG("atk", "Could not open command gate");
        return false;
    }

    setProperty("IsTouchpadEnabled", true);
    setProperty("Copyright", "Copyright © 2018-2020 Le Bao Hiep. All rights reserved.");

    extern kmod_info_t kmod_info;
    setProperty("AsusSMC-Version", kmod_info.version);
#ifdef DEBUG
    setProperty("AsusSMC-Build", "Debug");
#else
    setProperty("AsusSMC-Build", "Release");
#endif

    registerNotifications();

    registerVSMC();

    registerService();

    return true;
}

IOReturn AsusSMC::message(uint32_t type, IOService *provider, void *argument) {
    SYSLOG("lymosdebug", "AsusSMC [FnKey] message in\n");
    DBGLOG("lymosdebug", "Received message:type Provider");
    DBGLOG("atk", "Received message: %u Type %x Provider %s", *((uint32_t *)argument), type, provider ? provider->getName() : "unknown");
    
    switch (type) {
        case kIOACPIMessageDeviceNotification:
        {
            DBGLOG("tommydebug", " kIOACPIMessageDeviceNotification");
            if (directACPImessaging) {
                DBGLOG("tommydebug", "directACPImessaging");
                handleMessage(*((uint32_t *)argument));
            } else {
                DBGLOG("tommydebug", "no directACPImessaging in _WED");
                uint32_t event = *((uint32_t *)argument);
                OSNumber *arg = OSNumber::withNumber(event, 32);
                uint32_t res;
                atkDevice->evaluateInteger("_WED", &res, (OSObject **)&arg, 1);
                
                
                // OSNumber *arg2 = OSNumber::withNumber(*((uint16_t *)argument) / 16, 16);
                // atkDevice->evaluateObject("SKBV", NULL, (OSObject **)&arg2, 1);
                arg->release();
                handleMessage(res);
            }
            break;
        }

        case kSetKeyboardBacklightMessage:
        {
            DBGLOG("tommydebug", " kSetKeyboardBacklightMessage");
            if (hasKeyboardBacklight) {
                DBGLOG("tommydebug", " exec SKBV");
                OSNumber *arg = OSNumber::withNumber(*((uint16_t *)argument) / 16, 16);
                atkDevice->evaluateObject("SKBV", NULL, (OSObject **)&arg, 1);
                arg->release();
            }
            break;
        }

        default:
            return kIOReturnInvalid;
    }
    return kIOReturnSuccess;
}

在上述代码的message()方法中kSetKeyboardBacklightMessage下，匹配<key>ProductID</key><integer>6228</integer><key>VendorID</key><integer>2821</integer>
然后用匹配到的设备，发送report。 参考：void AsusHIDDriver::asus_kbd_backlight_set(uint8_t val) {
    DBGLOG("hid", "asus_kbd_backlight_set: val=%d", val);

    if (!(SUPPORT_KBD_BACKLIGHT & _kbd_function)) {
        DBGLOG("hid", "asus_kbd_backlight_set: Keyboard backlight is not supported");
      //  return;
    }

    uint8_t buf[] = { KBD_FEATURE_REPORT_ID, 0xba, 0xc5, 0xc4, 0x00 };
    buf[4] = val;

    IOBufferMemoryDescriptor *report = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, sizeof(buf));
    if (!report) {
        SYSLOG("hid", "asus_kbd_backlight_set: Could not allocate IOBufferMemoryDescriptor");
        return;
    }

    report->writeBytes(0, buf, sizeof(buf));

    hid_interface->setReport(report, kIOHIDReportTypeFeature, KBD_FEATURE_REPORT_ID);

    report->release();
}
