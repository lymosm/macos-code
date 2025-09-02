// File: AsusKeys.cpp
#include "AsusKeys.hpp"
#include "AsusKeysUserClient.hpp" // userclient declaration (assumed present)

#define super IOService
OSDefineMetaClassAndStructors(AsusKeys, IOService)

bool AsusKeys::init(OSDictionary* dict) {
    bool ok = IOService::init(dict);
    if (ok) {
        IOLog("tommydebug: AsusKeys::init\n");
        _provider = nullptr;
        _workLoop = nullptr;
        _commandGate = nullptr;
        _acpiNotifier = nullptr;
        _ec0Device = nullptr;
        _atkDevice = nullptr;

    }
    return ok;
}

void AsusKeys::free() {
    IOLog("tommydebug: AsusKeys::free\n");

    if (_acpiNotifier) {
        // notifier will be removed in stop() normally; safe-guard
        _acpiNotifier->remove();
        _acpiNotifier = nullptr;
    }

    if (_ec0Device) {
        _ec0Device->release();
        _ec0Device = nullptr;
    }
    if (_atkDevice) {   // 释放 ATKD
        _atkDevice->release();
        _atkDevice = nullptr;
    }

    if (_commandGate) {
        if (_workLoop) _workLoop->removeEventSource(_commandGate);
        _commandGate->release();
        _commandGate = nullptr;
    }

    // Do NOT release _workLoop unless you retained it explicitly
    // if (_workLoop) { _workLoop->release(); _workLoop = nullptr; }

    if (_provider) {
        _provider->release();
        _provider = nullptr;
    }

    IOService::free();
}

IOService* AsusKeys::probe(IOService* provider, SInt32* score) {
    IOService* res = super::probe(provider, score);
    IOLog("tommydebug: AsusKeys::probe\n");
    return res;
}

bool AsusKeys::start(IOService* provider) {
    if (!super::start(provider)) return false;

    _provider = provider;
    if (_provider) _provider->retain();

    IOLog("tommydebug: AsusKeys::start - registering service\n");

    // 获取 workloop 和创建 command gate
    _workLoop = getWorkLoop();
    if (!_workLoop) {
        IOLog("tommydebug: Failed to get workloop\n");
        return false;
    }
    // getWorkLoop() normally returns a retained workloop for the service; do not retain again

    _commandGate = IOCommandGate::commandGate(this);
    if (!_commandGate) {
        IOLog("tommydebug: Could not create command gate\n");
        return false;
    }

    if (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess) {
        IOLog("tommydebug: Could not add command gate to workloop\n");
        _commandGate->release();
        _commandGate = nullptr;
        return false;
    }

    // 使用异步匹配通知，避免在 start() 期间做同步枚举导致 catalog 锁死
    {
        OSDictionary* match = IOService::serviceMatching("IOACPIPlatformDevice");
        if (match) {
            // gIOMatchedNotification 是异步回调，不会在匹配目录锁持有期间执行你的 handler
            _acpiNotifier = IOService::addMatchingNotification(gIOMatchedNotification,
                                                              match,
                                                              AsusKeys::acpiPublishHandler,
                                                              this,
                                                              nullptr);
            match->release();
            IOLog("tommydebug: AsusKeys::start - registered ACPI publish notifier\n");
        } else {
            IOLog("tommydebug: AsusKeys::start - failed to create ACPI matching dict\n");
        }
    }

    registerService();

    IOLog("tommydebug: AsusKeys::start completed\n");
    return true;
}

void AsusKeys::stop(IOService* provider) {
    IOLog("tommydebug: AsusKeys::stop\n");

    // remove notifier first
    if (_acpiNotifier) {
        _acpiNotifier->remove();
        _acpiNotifier = nullptr;
    }

    if (_commandGate) {
        if (_workLoop) _workLoop->removeEventSource(_commandGate);
        _commandGate->release();
        _commandGate = nullptr;
    }

    if (_ec0Device) {
        _ec0Device->release();
        _ec0Device = nullptr;
    }
    if (_atkDevice) {   // 释放 ATKD
        _atkDevice->release();
        _atkDevice = nullptr;
    }

    if (_provider) {
        _provider->release();
        _provider = nullptr;
    }

    super::stop(provider);
}

IOReturn AsusKeys::newUserClient(task_t owningTask,
                                 void* securityID,
                                 UInt32 type,
                                 OSDictionary* properties,
                                 IOUserClient** handler) {
    if (!handler) return kIOReturnBadArgument;

    IOLog("tommydebug: AsusKeys::newUserClient called\n");

    AsusKeysUserClient* client = OSTypeAlloc(AsusKeysUserClient);
    if (!client) {
        IOLog("tommydebug: AsusKeys::newUserClient - OSTypeAlloc failed\n");
        return kIOReturnNoMemory;
    }

    if (!client->initWithTask(owningTask, securityID, type)) {
        IOLog("tommydebug: AsusKeys::newUserClient - initWithTask failed\n");
        client->release();
        return kIOReturnBadArgument;
    }

    if (!client->attach(this)) {
        IOLog("tommydebug: AsusKeys::newUserClient - attach failed\n");
        client->release();
        return kIOReturnUnsupported;
    }

    if (!client->start(this)) {
        IOLog("tommydebug: AsusKeys::newUserClient - start failed\n");
        client->detach(this);
        client->release();
        return kIOReturnUnsupported;
    }

    *handler = client;
    IOLog("tommydebug: AsusKeys::newUserClient - success\n");
    return kIOReturnSuccess;
}

bool AsusKeys::acpiPublishHandler(void* target, void* /*refCon*/, IOService* newService, IONotifier* /*notifier*/) {
    if (!target || !newService) return true; // keep notifier

    AsusKeys* self = OSDynamicCast(AsusKeys, (OSObject*)target);
    if (!self) return true;

    IOACPIPlatformDevice* dev = OSDynamicCast(IOACPIPlatformDevice, newService);
    if (!dev) return true;

    // call instance method to handle the published device
    self->onACPIDevicePublished(dev);

    return true; // keep receiving notifications
}

void AsusKeys::onACPIDevicePublished(IOACPIPlatformDevice* dev) {
    if (!dev) return;
    
    const char* name = dev->getName();
        if (!name) return;

        // 保存 EC0
        if (strcmp(name, "EC0") == 0 && !_ec0Device) {
            dev->retain();
            _ec0Device = dev;
            IOLog("tommydebug: Bound EC0 device\n");
            return;
        }

        // 保存 ATKD
        if (strcmp(name, "ATKD") == 0 && !_atkDevice) {
            dev->retain();
            _atkDevice = dev;
            IOLog("tommydebug: Bound ATKD device\n");
            return;
        }

    /*
    // 如果已经绑定了 EC，就不重复绑定
    if (_ec0Device) return;

    // 这里 dev 是“借用”，如果要保存必须 retain
    dev->retain();

    // 只作存在性验证，不主动触发固件事件
    IOReturn v = dev->validateObject("_Q13");
    if (v == kIOReturnSuccess) {
        _ec0Device = dev; // 保持引用
        IOLog("tommydebug: Bound EC device (validate _Q13 success): %s\n", _ec0Device->getName());
    } else {
        IOLog("tommydebug: ACPI device validateObject(_Q13) failed: 0x%x - releasing\n", v);
        dev->release();
    }
    */
    
}

void AsusKeys::handleUserMessage(uint32_t usage, uint32_t page, int32_t pressed, uint32_t keycode) {
 //   IOLog("tommydebug: received usage=0x%x page=0x%x pressed=%d keycode=0x%x\n",
 //         usage, page, (int)pressed, keycode);

    // 这里假设 page==0xFF31 usage==0xC5 为 Fn+F3
    if (page == 0xFF31 && usage == 0xC5) {
        if (pressed) {
            // 使用 command gate 来在 workloop 上串行执行对硬件的交互
            if (_commandGate) {
                _commandGate->runAction([](OSObject* owner, void* /*arg0*/, void* /*arg1*/, void* /*arg2*/, void* /*arg3*/) -> IOReturn {
                    AsusKeys* self = OSDynamicCast(AsusKeys, owner);
                    if (!self) return kIOReturnBadArgument;
                    self->decrease_keyboard_backlight();
                    return kIOReturnSuccess;
                });
            } else {
                IOLog("tommydebug: command gate not available\n");
            }
        }
    }else if (page == 0xFF31 && usage == 0xC4) {
        if (pressed) {
            // 使用 command gate 来在 workloop 上串行执行对硬件的交互
            if (_commandGate) {
                _commandGate->runAction([](OSObject* owner, void* /*arg0*/, void* /*arg1*/, void* /*arg2*/, void* /*arg3*/) -> IOReturn {
                    AsusKeys* self = OSDynamicCast(AsusKeys, owner);
                    if (!self) return kIOReturnBadArgument;
                    self->increase_keyboard_backlight();
                    return kIOReturnSuccess;
                });
            } else {
                IOLog("tommydebug: command gate not available\n");
            }
        }
    }
}

IOReturn AsusKeys::evaluateAcpiFromUser(const char* device, const char* method, OSArray* params, OSObject** outResult) {
    IOACPIPlatformDevice* target = nullptr;
    IOLog("tommydebug: evaluateObject(%s.%s) in\n", device, method);
    // 简单查找 EC0 或 ATKD，后面可扩展
    if (strcmp(device, "EC0") == 0 && _ec0Device) {
        target = _ec0Device;
    } else if (strcmp(device, "ATKD") == 0 && _atkDevice) {
        target = _atkDevice;
    } else {
        IOLog("tommydebug: unknown device %s\n", device);
        return kIOReturnNotFound;
    }

    // 将 method 字符串转换为 OSSymbol（evaluateObject 接受 OSSymbol）
      //  OSSymbol* sym = OSSymbol::withCString(method);
       // if (!sym) return kIOReturnNoMemory;

        OSObject* result = nullptr;
        IOReturn ret = kIOReturnError;

        if (!params || params->getCount() == 0) {
            IOLog("tommydebug: evaluateObject(%s.%s) no params\n", device, method);
            // 无参数：传 nullptr + 0
            ret = target->evaluateObject(method, &result, nullptr, 0);
        } else {
            // 有参数：把 OSArray 的元素取出组成 OSObject* 数组并传入
            IOItemCount count = params->getCount();
            const IOItemCount MAX_PARAMS = 16; // 根据需要调整
            IOItemCount passCount = (count > MAX_PARAMS) ? MAX_PARAMS : count;
            OSObject* argv[MAX_PARAMS]; // 在栈上，数量固定为 MAX_PARAMS
            IOLog("tommydebug: method %s has %u params\n", method, (unsigned)passCount);

            for (IOItemCount i = 0; i < passCount; ++i) {
                OSObject* obj = params->getObject(i);
                        

                        if (OSNumber* num = OSDynamicCast(OSNumber, obj)) {
                            IOLog("tommydebug: param[%u] = OSNumber %llu\n", (unsigned)i, num->unsigned64BitValue());
                        } else if (OSString* str = OSDynamicCast(OSString, obj)) {
                            IOLog("tommydebug: param[%u] = OSString %s\n", (unsigned)i, str->getCStringNoCopy());
                        } else {
                            IOLog("tommydebug: param[%u] = <%s>\n", (unsigned)i, obj->getMetaClass()->getClassName());
                        }
                // getObject 不会增加引用计数（返回当前持有的指针），
                // 这里直接取出即可（保证 params 在调用时有效）
                argv[i] = obj;
            }
            
           

            // 调用带 params[] 和 count 的 overload
            ret = target->evaluateObject(method, &result, argv, passCount);
        }
    
    if (ret == kIOReturnSuccess) {
        IOLog("tommydebug: evaluateObject(%s.%s) success\n", device, method);
        
        if (result) *outResult = result; // 调用方要负责 release
    } else {
        IOLog("tommydebug: evaluateObject(%s.%s) failed: 0x%x\n", device, method, ret);
    }
    return ret;
}

void AsusKeys::decrease_keyboard_backlight() {
    if (!_ec0Device) {
        IOLog("tommydebugfn: Cannot call _Q13 - EC0 device not found\n");
        return;
    }

    // 重要：这里我们在 command gate 上调用 ACPI method
    // //IOLog("tommydebugfn: Calling _Q13 (from user request) in commandGate context\n");

    // 为安全起见，先用 evaluateObject 但不假设返回值类型
    OSObject* result = nullptr;
    IOReturn ret = _ec0Device->evaluateObject("_Q13", &result);
    if (ret == kIOReturnSuccess) {
        IOLog("tommydebugfn: evaluateObject(_Q13) returned success\n");
        if (result) {
            result->release();
            result = nullptr;
        }
    } else {
        IOLog("tommydebugfn: evaluateObject(_Q13) failed: 0x%x\n", ret);
    }
}

void AsusKeys::increase_keyboard_backlight() {
    if (!_ec0Device) {
        IOLog("tommydebugfn: Cannot call _Q14 - EC0 device not found\n");
        return;
    }

    // 重要：这里我们在 command gate 上调用 ACPI method
    IOLog("tommydebugfn: Calling _Q14 (from user request) in commandGate context\n");

    // 为安全起见，先用 evaluateObject 但不假设返回值类型
    OSObject* result = nullptr;
    IOReturn ret = _ec0Device->evaluateObject("_Q14", &result);
    if (ret == kIOReturnSuccess) {
        IOLog("tommydebugfn: evaluateObject(_Q14) returned success\n");
        if (result) {
            result->release();
            result = nullptr;
        }
    } else {
        IOLog("tommydebugfn: evaluateObject(_Q13) failed: 0x%x\n", ret);
    }
}
