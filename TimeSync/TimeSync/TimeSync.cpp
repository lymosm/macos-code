#include "TimeSync.hpp"
#include <libkern/libkern.h> // for printf & kprintf if needed
#include <IOKit/IOLib.h>


#define super IOService
OSDefineMetaClassAndStructors(TimeSync, IOService);

bool TimeSync::init(OSDictionary* dict) {
    if (!super::init(dict)) return false;
    IOLog("%s::init - lymos.dev.TimeSync\n", getName() ? getName() : "TimeSync");
    return true;
}

void TimeSync::free() {
    IOLog("%s::free\n", getName() ? getName() : "TimeSync");
    super::free();
}

IOService* TimeSync::probe(IOService* provider, SInt32* score) {
    IOLog("%s::probe (provider=%s)\n", getName() ? getName() : "TimeSync", provider ? provider->getName() : "<null>");
    return super::probe(provider, score);
}

bool TimeSync::start(IOService* provider) {
    IOLog("%s::start (provider=%s)\n", getName() ? getName() : "TimeSync", provider ? provider->getName() : "<null>");

    if (!super::start(provider)) {
        IOLog("%s::start - super::start failed\n", getName() ? getName() : "TimeSync");
        return false;
    }

    // Register service so user-space / matching can find it (no real functionality here)
    registerService();

    IOLog("%s::start - registered\n", getName() ? getName() : "TimeSync");
    return true;
}

void TimeSync::stop(IOService* provider) {
    IOLog("%s::stop (provider=%s)\n", getName() ? getName() : "TimeSync", provider ? provider->getName() : "<null>");
    super::stop(provider);
}

/*
 extern "C" kern_return_t TimeSync_start(kmod_info_t * ki, void *d) {
 IOLog("TimeSync_start loaded\n");
 return KERN_SUCCESS;
 }
 
 extern "C" kern_return_t TimeSync_stop(kmod_info_t *ki, void *d) {
 IOLog("TimeSync_stop unloaded\n");
 return KERN_SUCCESS;
 }
 */
