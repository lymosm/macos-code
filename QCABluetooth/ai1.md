黑苹果Tahoe开发高通AR3k蓝牙kext，目前已经通过QCABluetooth.kext完成了固件上传，另外想写多一个target: QCAUSBTransport让IOBluetoothFamily.kext attach，但是失败了。以下是QCAUSBTransport代码：
Info.plist:
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>$(DEVELOPMENT_LANGUAGE)</string>
    <key>CFBundleIdentifier</key>
    <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$(PRODUCT_NAME)</string>
    <key>CFBundleExecutable</key>
    <string>$(EXECUTABLE_NAME)</string>
    <key>CFBundlePackageType</key>
    <string>$(PRODUCT_BUNDLE_PACKAGE_TYPE)</string>
    <key>CFBundleVersion</key>
    <string>$(CURRENT_PROJECT_VERSION)</string>
    <key>CFBundleShortVersionString</key>
    <string>$(MARKETING_VERSION)</string>

    <key>IOKitPersonalities</key>
    <dict>
        <!-- personality 1: interface (首选匹配) -->
        <key>QCA USB Bluetooth Transport (Interface)</key>
        <dict>
            <key>CFBundleIdentifier</key>
            <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
            <key>IOClass</key>
            <string>QCAUSBTransport</string>
            <key>IOProviderClass</key>
            <string>IOUSBHostInterface</string>
            <key>bInterfaceClass</key>
            <integer>224</integer>
            <key>bInterfaceSubClass</key>
            <integer>1</integer>
            <key>bInterfaceProtocol</key>
            <integer>1</integer>
            <key>IOProbeScore</key>
            <integer>200</integer>
        </dict>

        <!-- personality 2: device (备用) — 让 kext 在 device 出现时就加载 -->
        <key>QCA USB Bluetooth Transport (Device)</key>
        <dict>
            <key>CFBundleIdentifier</key>
            <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
            <key>IOClass</key>
            <string>QCAUSBTransport</string>
            <key>IOProviderClass</key>
            <string>IOUSBHostDevice</string>

            <!-- 用你给的 vendor/product 限定，避免错配 -->
            <key>idVendor</key>
            <integer>1226</integer>
            <key>idProduct</key>
            <integer>12312</integer>
            <key>IOProbeScore</key>
            <integer>200</integer>
        </dict>
    </dict>


    <key>OSBundleRequired</key>
    <string>Root</string>

    <key>OSBundleLibraries</key>
    <dict>
        <key>com.apple.kpi.bsd</key>
            <string>8.0.0</string>
            <key>com.apple.iokit.IOUSBHostFamily</key>
            <string>1.2</string>
            <key>com.apple.iokit.IOBluetoothFamily</key>
            <string>9.0.0</string>
            <key>com.apple.kpi.iokit</key>
            <string>16.7</string>
            <key>com.apple.kpi.libkern</key>
            <string>16.7</string>
            <key>com.apple.kpi.mach</key>
            <string>16.7</string>
            <key>com.apple.kpi.unsupported</key>
            <string>8.0.0</string>
    </dict>
</dict>
</plist>

QCAUSBTransport.cpp:
#include "QCAUSBTransport.hpp"
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(QCAUSBTransport, IOBluetoothHostControllerUSBTransport)

bool QCAUSBTransport::init(OSDictionary* dict) {
    if (!IOBluetoothHostControllerUSBTransport::init(dict)) return false;
    IOLog("tommybts: QCAUSBTransport::init\n");
    fInterface = nullptr;
    fDevice = nullptr;
    return true;
}

IOUSBHostInterface* QCAUSBTransport::findReadyInterface(IOService* device, uint32_t maxWaitMs, uint32_t pollMs) {
    if (!device) return nullptr;

    uint32_t elapsed = 0;
    while (elapsed < maxWaitMs) {
        OSIterator* iter = device->getChildIterator(gIOServicePlane);
        if (iter) {
            IORegistryEntry* child = nullptr;
            while ((child = (IORegistryEntry*)iter->getNextObject())) {
                IOUSBHostInterface* iface = OSDynamicCast(IOUSBHostInterface, child);
                if (iface) {
                    OSObject* val = iface->getProperty("QCAFirmwareLoaded");
                    if (val == kOSBooleanTrue) {
                        IOLog("tommybts: findReadyInterface - found ready interface %s\n",
                              iface->getName() ? iface->getName() : "unknown");
                        iter->release();
                        return iface;
                    }
                }
            }
            iter->release();
        }
        IOSleep(pollMs);
        elapsed += pollMs;
    }

    return nullptr;
}

bool QCAUSBTransport::start(IOService* provider) {
    IOLog("tommybts: QCAUSBTransport::start provider=%p name=%s\n", provider, provider ? provider->getName() : nullptr);

    IOUSBHostInterface* ifaceProvider = OSDynamicCast(IOUSBHostInterface, provider);
    IOUSBHostDevice* deviceProvider = OSDynamicCast(IOUSBHostDevice, provider);

    if (ifaceProvider) {
        IOLog("tommybts: provider is IOUSBHostInterface\n");
        fInterface = ifaceProvider;
    } else if (deviceProvider) {
        IOLog("tommybts: provider is IOUSBHostDevice - searching for interface child\n");
        IOUSBHostInterface* found = findReadyInterface(deviceProvider, /*maxWaitMs*/10000, /*pollMs*/200);
        if (!found) {
            IOLog("tommybts: no ready interface found, try to find any interface child\n");
            OSIterator* iter = deviceProvider->getChildIterator(gIOServicePlane);
            if (iter) {
                IORegistryEntry* child = nullptr;
                while ((child = (IORegistryEntry*)iter->getNextObject())) {
                    IOUSBHostInterface* iface = OSDynamicCast(IOUSBHostInterface, child);
                    if (iface) {
                        found = iface;
                        break;
                    }
                }
                iter->release();
            }
        }
        if (found) {
            fInterface = found;
            fDevice = deviceProvider;
            IOLog("tommybts: chosen interface %s\n", fInterface->getName() ? fInterface->getName() : "unknown");
        } else {
            IOLog("tommybts: no interface child found under device - abort start\n");
            return false;
        }
    } else {
        IOLog("tommybts: provider is neither IOUSBHostInterface nor IOUSBHostDevice - abort\n");
        return false;
    }

    IOService* startProvider = (IOService*)fInterface;
    if (!IOBluetoothHostControllerUSBTransport::start(startProvider)) {
        IOLog("tommybts: super::start failed!\n");
        return false;
    }

    registerService();
    IOLog("tommybts: QCAUSBTransport::registerService done\n");

    return true;
}

void QCAUSBTransport::stop(IOService* provider) {
    IOLog("tommybts: QCAUSBTransport::stop\n");
    IOBluetoothHostControllerUSBTransport::stop(provider);
}

IOReturn QCAUSBTransport::message(UInt32 type, IOService* provider, void* argument) {
    IOLog("tommybts: QCAUSBTransport::message type=%u provider=%p\n", type, provider);
    return IOBluetoothHostControllerUSBTransport::message(type, provider, argument);
}

实际验证测试结果是：IOBluetoothFamily.kext并没有暴露IOBluetoothHostControllerUSBTransport，所以找不到IOBluetoothHostControllerUSBTransport符号，导致QCAUSBTransport.kext无法加载。
问：
1. 如何解决这个情况？
2. 或者有什么其他方案可以实现驱动高通蓝牙？
请用中文回复谢谢！
