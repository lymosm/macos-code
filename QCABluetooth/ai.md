黑苹果，开发高通蓝牙驱动kext，通过opencore加载，为什么无法加载？没有看到相关报错,IOLog一个日志也没看到，通过windows看到设备是：
Atheros Buletooth：USB\VID_04CA&PID_3018\5&16BDD27B&0&9
  以下是代码：
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

    <key>OSBundleRequired</key>
    <string>Root</string>

    <key>IOKitPersonalities</key>
    <dict>
        <key>QCABluetooth USB</key>
        <dict>
            <key>CFBundleIdentifier</key>
            <string>$(PRODUCT_BUNDLE_IDENTIFIER)</string>
            <key>IOClass</key>
            <string>QCABluetoothUSB</string>
            <key>IOProviderClass</key>
            <string>IOUSBHostDevice</string>
            
            <key>idVendor</key>
            <integer>1226</integer>
            <key>idProduct</key>
            <integer>12312</integer>
            
            <!-- 
            <key>IOResourceMatch</key>
            <string>IOKit</string>
            -->
        </dict>
    </dict>

    <key>OSBundleLibraries</key>
    <dict>
        <key>com.apple.kpi.bsd</key>
        <string>19.0.0</string>
        <key>com.apple.kpi.iokit</key>
        <string>19.0.0</string>
        <key>com.apple.kpi.libkern</key>
        <string>19.0.0</string>
        <key>com.apple.kpi.unsupported</key>
        <string>8.0.0</string>
    </dict>
</dict>
</plist>

QCABluetooth.hpp:
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#define super IOService

class QCABluetoothUSB : public IOService {
    OSDeclareDefaultStructors(QCABluetoothUSB)

private:
    IOUSBHostDevice *   _device;
    IOUSBHostInterface * _interface;
    IOWorkLoop *         _workloop;
    IOCommandGate *      _cmdGate;

public:
    virtual bool init(OSDictionary* dict) override;
    virtual void free() override;
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // Minimal helpers
    bool findAndOpenInterface();
    bool uploadFirmware();
    void handleUSBData();
};

QCABluetooth.cpp:
#include "QCABluetooth.hpp"


OSDefineMetaClassAndStructors(QCABluetoothUSB, IOService)

bool QCABluetoothUSB::init(OSDictionary* dict) {
    IOLog("tommybt: init in\n");
    if (!super::init(dict)) return false;
    _device = nullptr;
    _interface = nullptr;
    _workloop = nullptr;
    _cmdGate = nullptr;
    return true;
}

void QCABluetoothUSB::free() {
    if (_cmdGate) {
        _cmdGate->release();
        _cmdGate = nullptr;
    }
    if (_workloop) {
        _workloop->release();
        _workloop = nullptr;
    }
    super::free();
}

IOService* QCABluetoothUSB::probe(IOService* provider, SInt32* score) {
    IOLog("tommybt: probe in\n");
    IOService* res = super::probe(provider, score);
    // Optionally adjust score
    return res;
}

bool QCABluetoothUSB::start(IOService* provider) {
    IOLog("tommybt: start in\n");
    if (!super::start(provider)) return false;

    _device = OSDynamicCast(IOUSBHostDevice, provider);
    if (!_device) {
        IOLog("tommybt: provider is not IOUSBHostDevice\n");
        return false;
    }

    _workloop = IOWorkLoop::workLoop();
    if (!_workloop) {
        IOLog("tommybt: failed to create workloop\n");
        return false;
    }

    _cmdGate = IOCommandGate::commandGate(this);
    if (!_cmdGate) {
        IOLog("tommybt: failed to create command gate\n");
        _workloop->release();
        _workloop = nullptr;
        return false;
    }

    if (_workloop->addEventSource(_cmdGate) != kIOReturnSuccess) {
        IOLog("tommybt: failed to add command gate to workloop\n");
        _cmdGate->release();
        _cmdGate = nullptr;
        _workloop->release();
        _workloop = nullptr;
        return false;
    }

    // Find and open the correct USB interface (bulk/interrupt endpoints)
    if (!findAndOpenInterface()) {
        IOLog("tommybt: findAndOpenInterface failed\n");
        stop(provider);
        return false;
    }

    // Upload firmware if required
    if (!uploadFirmware()) {
        IOLog("tommybt: firmware upload failed (may still work if not required)\n");
        // continue — some devices might not require external firmware
    }

    // TODO: set up USB pipes and callbacks to receive HCI events
    IOLog("tommybt: started\n");
    registerService();
    return true;
}

void QCABluetoothUSB::stop(IOService* provider) {
    IOLog("tommybt: stop\n");
    // Close interface, remove event sources
    if (_workloop && _cmdGate) {
        _workloop->removeEventSource(_cmdGate);
    }
    if (_cmdGate) {
        _cmdGate->release();
        _cmdGate = nullptr;
    }
    if (_workloop) {
        _workloop->release();
        _workloop = nullptr;
    }
    super::stop(provider);
}

bool QCABluetoothUSB::findAndOpenInterface() {
    // Very simplified: pick first interface. In production enumerate interfaces
    // OSObject* ifaces = _device->getDeviceDescriptor();
    // Proper implementation should call _device->copyMatchingInterface() etc.
    // This is a placeholder to show where you'd open the interface and claim endpoints.
    IOLog("tommybt: findAndOpenInterface (placeholder)\n");
    return true;
}

bool QCABluetoothUSB::uploadFirmware() {
    // Load firmware resource from kext bundle or filesystem and send via control/bulk transfer
    IOLog("tommybt: uploadFirmware (placeholder)\n");
    return true;
}

void QCABluetoothUSB::handleUSBData() {
    // Called when input endpoint has HCI events
}
