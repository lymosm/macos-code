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

