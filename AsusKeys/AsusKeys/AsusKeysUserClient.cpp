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
    { (IOExternalMethodAction)&AsusKeysUserClient::sLogUsage,
      4, 0, 0, 0 },

    { (IOExternalMethodAction)&AsusKeysUserClient::sEvaluateAcpiMethod,
      0, sizeof(AcpiMethodInput), 0, sizeof(AcpiMethodOutput) }
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

IOReturn AsusKeysUserClient::sEvaluateAcpiMethod(OSObject* target, void* reference, IOExternalMethodArguments* args) {
    AsusKeysUserClient* self = OSDynamicCast(AsusKeysUserClient, target);
    if (!self) return kIOReturnBadArgument;
    return self->evaluateAcpiMethod(args);
}

IOReturn AsusKeysUserClient::evaluateAcpiMethod(IOExternalMethodArguments* args) {
    IOLog("tommydebug: evaluateAcpiMethod in\n");
    if (!args) return kIOReturnBadArgument;

        IOLog("tommydebug: evaluateAcpiMethod in\n");
        IOLog("tommydebug: input size=%u expected=%lu\n",
              args->structureInputSize, sizeof(AcpiMethodInput));

        if (args->structureInputSize < sizeof(AcpiMethodInput))
            return kIOReturnBadArgument;
    AcpiMethodOutput* output = (AcpiMethodOutput*)args->structureOutput;
    memset(output, 0, sizeof(AcpiMethodOutput));

    AcpiMethodInput* input = (AcpiMethodInput*)args->structureInput;
    IOLog("tommydebug: evaluateAcpiMethod request device=%s method=%s\n", input->device, input->method);

    if (!fProvider) return kIOReturnNotAttached;
    // 构造参数数组
        OSArray* params = nullptr;
        if (input->argCount > 0) {
            params = OSArray::withCapacity(input->argCount);
            for (uint32_t i = 0; i < input->argCount; i++) {
                AcpiMethodArg* arg = &input->args[i];
                OSObject* obj = nullptr;
                if (arg->type == ACPI_ARG_TYPE_INT) {
                    obj = OSNumber::withNumber(arg->intValue, 32);
                } else if (arg->type == ACPI_ARG_TYPE_STRING) {
                    obj = OSString::withCString(arg->strValue);
                }
                if (obj) {
                    params->setObject(obj);
                    obj->release();
                }
            }
        }
    OSObject* result = nullptr;
    IOReturn ret = fProvider->evaluateAcpiFromUser(input->device, input->method, params, &result);

        if (params) params->release();
    
    /// 填充返回值
    if (result) {
        if (OSNumber* num = OSDynamicCast(OSNumber, result)) {
            output->type = ACPI_RET_INT;
            output->intValue = num->unsigned64BitValue();
            IOLog("tommydebug: ACPI returned int=%llu\n", output->intValue);
        } else if (OSString* str = OSDynamicCast(OSString, result)) {
            output->type = ACPI_RET_STRING;
            strncpy(output->strValue, str->getCStringNoCopy(), sizeof(output->strValue)-1);
            IOLog("tommydebug: ACPI returned string=%s\n", output->strValue);
        } else {
            output->type = ACPI_RET_NONE;
            IOLog("tommydebug: ACPI returned unsupported type\n");
        }
        result->release();
    } else {
        output->type = ACPI_RET_NONE;
        IOLog("tommydebug: ACPI returned no value\n");
    }

    args->structureOutputSize = sizeof(AcpiMethodOutput);
        return ret;
}





IOReturn AsusKeysUserClient::externalMethod(uint32_t selector,
                                           IOExternalMethodArguments* arguments,
                                           IOExternalMethodDispatch* dispatch,
                                           OSObject* target,
                                           void* reference) {
 //   IOLog("tommydebug: entering externalMethod with selector %u\n", selector);

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
  //  IOLog("tommydebug: sLogUsage in\n");
    if (!self) return kIOReturnBadArgument;
 //   IOLog("tommydebug: go to logUsage\n");
    return self->logUsage(args);
}

IOReturn AsusKeysUserClient::logUsage(IOExternalMethodArguments* args) {
   // IOLog("tommydebug: logUsage in\n");
    if (!args || args->scalarInputCount < 4) {
            IOLog("tommydebug: logUsage - not enough scalars\n");
            return kIOReturnBadArgument;
        }

        uint32_t usage = (uint32_t)args->scalarInput[0];
        uint32_t page     = (uint32_t)args->scalarInput[1];
        uint32_t pressed   = (uint32_t)args->scalarInput[2];
        uint32_t keycode   = (uint32_t)args->scalarInput[3];

      //  IOLog("tommydebug: logUsage scalar: usage=0x%x page=0x%x pressed=%u keycode=0x%x\n",
      //        usage, page, pressed, keycode);

        if (fProvider) {
            fProvider->handleUserMessage(usage, page, pressed, keycode);
            return kIOReturnSuccess;
        }
        return kIOReturnNotAttached;
}
