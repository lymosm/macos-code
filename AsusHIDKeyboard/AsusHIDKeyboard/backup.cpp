//
//  s.hpp
//  AsusATKD
//
//  Created by lymos on 2025/8/29.
//


#include "AsusHIDKeyboard.hpp"

// define super for convenience
#define super IOService
OSDefineMetaClassAndStructors(AsusHIDKeyboard, IOService);

bool AsusHIDKeyboard::init(OSDictionary* dict)
{
    if (!super::init(dict)) return false;
    _workLoop = nullptr;
    _cmdGate = nullptr;
    _provider = nullptr;
    IOLog("tommydebug: AsusATKD::init\n");
    return true;
}

void AsusHIDKeyboard::free(void)
{
    IOLog("tommydebug: AsusATKD::free\n");
    if (_cmdGate) {
        if (_workLoop) _workLoop->removeEventSource(_cmdGate);
        _cmdGate->release();
        _cmdGate = nullptr;
    }
    if (_workLoop) {
        _workLoop->release();
        _workLoop = nullptr;
    }
    super::free();
}

IOService* AsusHIDKeyboard::probe(IOService* provider, SInt32* score)
{
    IOLog("tommydebug: AsusATKD::probe provider=%s\n",
          provider ? provider->getName() : "(null)");
    // keep default scoring unless you want to increase it
    return super::probe(provider, score);
}

bool AsusHIDKeyboard::start(IOService* provider)
{ 
    if (!super::start(provider)) return false;
    _provider = provider;

    // create / retain workloop and command gate
    _workLoop = getWorkLoop();
    if (_workLoop) _workLoop->retain();
    if (_workLoop) {
        _cmdGate = IOCommandGate::commandGate(this);
        if (_cmdGate) _workLoop->addEventSource(_cmdGate);
    }

    // read vendor/product (if available) from provider properties
    int vid = -1, pid = -1;
    if (provider) {
        OSObject* v = provider->getProperty(kIOHIDVendorIDKey);
        OSObject* p = provider->getProperty(kIOHIDProductIDKey);
        if (v && OSDynamicCast(OSNumber, v))
            vid = (int)OSDynamicCast(OSNumber, v)->unsigned32BitValue();
        if (p && OSDynamicCast(OSNumber, p))
            pid = (int)OSDynamicCast(OSNumber, p)->unsigned32BitValue();
    }

    IOLog("tommydebug: AsusATKD::start provider=%s vid=0x%04x pid=0x%04x\n",
          provider ? provider->getName() : "(null)",
          (vid >= 0 ? vid : 0), (pid >= 0 ? pid : 0));

    // check match — only proceed to register hooks if matches expected VID/PID
    if (vid == ASUS_VENDOR_ID && pid == ASUS_PRODUCT_ID) {
        IOLog("tommydebug: AsusATKD matched target device (vid=0x%04x pid=0x%04x)\n", vid, pid);

        /*
         * NOTE:
         * 这里我们示例性的说明如何去注册 HID 回调，但实际注册函数
         * 与 macOS / HIDFamily 版本强相关（有的版本 provider 是 IOHIDDevice，
         * 有的 provider 是 IOHIDInterface，且方法名也可能不同）。
         *
         * 常见做法（仅作说明）：
         *   if (IOHIDDevice* hid = OSDynamicCast(IOHIDDevice, provider)) {
         *       // 某些 SDK 里有 hid->registerInputReport(...) 或类似方法
         *       hid->registerInputReport(yourBuffer, bufferLen, InputReportCallback, this);
         *   }
         *
         * 因为这些 API 在不同 SDK/系统版本存在差异，这里不直接调用以避免编译失败。
         * 你应当：
         *  1) 在目标机器上用 ioreg / IORegistryExplorer 看 provider 的类名（IOHIDDevice / IOHIDInterface / USBDevice 等）
         *  2) 在对应 SDK 中查找该类所支持的注册回调函数，并在此处加入注册代码
         *  3) 回调实现（InputReportCallback）中解析 reportData，**只打印 usage page / usage / value**
         */
        
        IOLog("tommydebug: AsusATKD - Please adapt and call provider's HID registration API here.\n");
    } else {
        IOLog("tommydebug: AsusATKD provider VID/PID mismatch; not registering HID callbacks.\n");
    }

    registerService(); // advertise ourselves
    IOLog("tommydebug: AsusATKD::start completed\n");
    return true;
}

void AsusATAsusHIDKeyboardKD::stop(IOService* provider)
{
    IOLog("tommydebug: AsusATKD::stop\n");
    // if you registered callbacks on provider, unregister them here
    if (_cmdGate && _workLoop) {
        _workLoop->removeEventSource(_cmdGate);
    }
    if (_cmdGate) { _cmdGate->release(); _cmdGate = nullptr; }
    if (_workLoop) { _workLoop->release(); _workLoop = nullptr; }
    _provider = nullptr;
    super::stop(provider);
}

// Static callback stub - adapt signature to provider API used
void AsusHIDKeyboard::InputReportCallback(void* target,
                                   IOReturn status,
                                   void* refCon,
                                   UInt32 reportType,
                                   UInt32 reportID,
                                   const void* reportData,
                                   UInt32 reportLength)
{
    (void)status; (void)refCon; (void)reportType; (void)reportID; (void)reportData; (void)reportLength;

    AsusHIDKeyboard* self = OSDynamicCast(AsusHIDKeyboard, (OSObject*)target);
    if (!self) return;

    // TODO: 在这里解析 reportData -> 找到 usagePage / usage / value
    // **重要安全约束**：不要把扫描码/键位转换为字符并输出。仅输出 usage page / usage / 数值。
    //
    // 示例演示（占位，不代表真实解析）：
    uint32_t usagePage = 0xFF00; // 占位（厂商页面示例）
    uint32_t usage     = 0x0001;
    int64_t  value     = 1;

    // 过滤 Keyboard page (0x07)
    if (usagePage == 0x07) return;

    self->safeLogUsage(usagePage, usage, value);
}

void AsusHIDKeyboard::safeLogUsage(uint32_t usagePage, uint32_t usage, int64_t value)
{
    // concise kernel log with your prefix
    IOLog("tommydebug: AsusATKD usage_page=0x%04x usage=0x%04x value=%lld\n",
          usagePage, usage, (long long)value);
}
