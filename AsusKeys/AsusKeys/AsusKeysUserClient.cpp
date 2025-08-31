//
//  AsusKeysUserClient.cpp
//  AsusKeys
//
//  Created by lymos on 2025/8/30.
//

// AsusKeysUserClient.cpp
#include "AsusKeysUserClient.hpp"
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(AsusKeysUserClient, IOUserClient)

// dispatch table：selector 0 -> sLogUsage
const IOExternalMethodDispatch AsusKeysUserClient::sMethods[AsusKeysUserClient::kNumberOfMethods] = {
    { (IOExternalMethodAction) &AsusKeysUserClient::sLogUsage,  /* action */
      4,                 /* checkScalarInputCount */
      0, /* checkStructureInputSize */
      0, 0 }             /* output counts/sizes */
};

bool AsusKeysUserClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type) {
    if (!IOUserClient::initWithTask(owningTask, securityToken, type)) return false;
    fTask = owningTask;
    fProvider = NULL;
    IOLog("tommydebug: AsusKeysUserClient::initWithTask\n");
    return true;
}

bool AsusKeysUserClient::start(IOService* provider) {
    if (!IOUserClient::start(provider)) return false;
    fProvider = OSDynamicCast(AsusKeys, provider);
    if (!fProvider) {
        IOLog("tommydebug: AsusKeysUserClient::start - provider is not AsusKeys\n");
        return false;
    }
    fProvider->retain();
    IOLog("tommydebug: AsusKeysUserClient::start - connected to provider\n");
    return true;
}

void AsusKeysUserClient::stop(IOService* provider) {
    IOLog("tommydebug: AsusKeysUserClient::stop\n");
    if (fProvider) {
        fProvider->release();
        fProvider = NULL;
    }
    IOUserClient::stop(provider);
}

IOReturn AsusKeysUserClient::clientClose(void) {
    IOLog("tommydebug: AsusKeysUserClient::clientClose\n");
    // 主动终止自身
    terminate(); // 触发 stop/free 等
    return kIOReturnSuccess;
}

IOReturn AsusKeysUserClient::externalMethod(uint32_t selector,
                                           IOExternalMethodArguments* arguments,
                                           IOExternalMethodDispatch* dispatch,
                                           OSObject* target,
                                           void* reference) {
    IOLog("tommydebug: entering externalMethod with selector %u\n", selector);

    if (selector >= kNumberOfMethods) return kIOReturnBadArgument;

    // 使用我们自己的 dispatch 表，并把 target 设为 this
    dispatch = (IOExternalMethodDispatch*)&sMethods[selector];
    target = this;

    // 调用父类实现来处理 dispatch
    return IOUserClient::externalMethod(selector, arguments, dispatch, target, reference);
}

// static dispatch wrapper
IOReturn AsusKeysUserClient::sLogUsage(OSObject* target, void* reference, IOExternalMethodArguments* args) {
    AsusKeysUserClient* self = OSDynamicCast(AsusKeysUserClient, target);
    IOLog("tommydebug: sLogUsage in\n");
    if (!self) return kIOReturnBadArgument;
    IOLog("tommydebug: go to logUsage\n");
    return self->logUsage(args);
}

IOReturn AsusKeysUserClient::logUsage(IOExternalMethodArguments* args) {
    IOLog("tommydebug: logUsage in\n");
    if (!args || args->scalarInputCount < 4) {
            IOLog("tommydebug: logUsage - not enough scalars\n");
            return kIOReturnBadArgument;
        }

        uint32_t usage = (uint32_t)args->scalarInput[0];
        uint32_t page     = (uint32_t)args->scalarInput[1];
        uint32_t pressed   = (uint32_t)args->scalarInput[2];
        uint32_t keycode   = (uint32_t)args->scalarInput[3];

        IOLog("tommydebug: logUsage scalar: usage=0x%x page=0x%x pressed=%u keycode=0x%x\n",
              usage, page, pressed, keycode);

        if (fProvider) {
            fProvider->handleUserMessage(usage, page, pressed, keycode);
            return kIOReturnSuccess;
        }
        return kIOReturnNotAttached;
}
