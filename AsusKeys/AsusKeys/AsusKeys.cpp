// AsusKeys.cpp
#include "AsusKeys.hpp"
#include "AsusKeysUserClient.hpp" // 必须包含 userclient 的定义
#include <libkern/OSAtomic.h>

OSDefineMetaClassAndStructors(AsusKeys, IOService)

bool AsusKeys::init(OSDictionary* dict) {
    bool ok = IOService::init(dict);
    if (ok) {
        IOLog("tommydebug: AsusKeys::init\n");
    }
    return ok;
}

void AsusKeys::free() {
    IOLog("tommydebug: AsusKeys::free\n");
    IOService::free();
}

IOService* AsusKeys::probe(IOService* provider, SInt32* score) {
    IOLog("tommydebug: AsusKeys::probe\n");
    // 简单返回 this（Info.plist 控制匹配）
    return this;
}

bool AsusKeys::start(IOService* provider) {
    if (!IOService::start(provider)) return false;

    _provider = provider;
    if (_provider) _provider->retain();

    IOLog("tommydebug: AsusKeys::start - registering service\n");

    // 将 service 注册，让用户态可以用 IOServiceMatching("AsusKeys") 找到
    registerService();

    IOLog("tommydebug: AsusKeys::start completed\n");
    return true;
}

void AsusKeys::stop(IOService* provider) {
    IOLog("tommydebug: AsusKeys::stop\n");
    if (_provider) {
        _provider->release();
        _provider = nullptr;
    }
    IOService::stop(provider);
}

// newUserClient: 当用户态调用 IOServiceOpen 时被调用，负责创建并返回 IOUserClient 子类对象
IOReturn AsusKeys::newUserClient(task_t owningTask,
                                 void* securityID,
                                 UInt32 type,
                                 OSDictionary* properties,
                                 IOUserClient** handler) {
    if (!handler) return kIOReturnBadArgument;

    IOLog("tommydebug: AsusKeys::newUserClient called\n");

    // 分配用户客户端实例
    AsusKeysUserClient* client = OSTypeAlloc(AsusKeysUserClient);
    if (!client) {
        IOLog("tommydebug: AsusKeys::newUserClient - OSTypeAlloc failed\n");
        return kIOReturnNoMemory;
    }

    // 调用 initWithTask（注意：如果你的 AsusKeysUserClient::initWithTask 签名不同，请调整这里）
    if (!client->initWithTask(owningTask, securityID, type)) {
        IOLog("tommydebug: AsusKeys::newUserClient - initWithTask failed\n");
        client->release();
        return kIOReturnBadArgument;
    }

    // attach 到 provider（this）
    if (!client->attach(this)) {
        IOLog("tommydebug: AsusKeys::newUserClient - attach failed\n");
        client->release();
        return kIOReturnUnsupported;
    }

    // start client（会调用 client->start(this)）
    if (!client->start(this)) {
        IOLog("tommydebug: AsusKeys::newUserClient - start failed\n");
        client->detach(this);
        client->release();
        return kIOReturnUnsupported;
    }

    // 成功，返回 client（注意：不要在这里 release client）
    *handler = client;
    IOLog("tommydebug: AsusKeys::newUserClient - success\n");
    return kIOReturnSuccess;
}

void AsusKeys::handleUserMessage(uint32_t usage, uint32_t page, int32_t pressed, uint32_t keycode) {
    IOLog("tommydebug: received usage=0x%x page=0x%x pressed=%d keycode=0x%x\n",
          usage, page, (int)pressed, keycode);
    // 控制键盘背光
    // 检查是否是 Fn+F3 (usage=0xC5, page=0xFF31) 按键
        if (page == 0xFF31 && usage == 0xC5) {
            if (pressed) {
                IOLog("tommydebug: Decreasing keyboard backlight...\n");
                decrease_keyboard_backlight();
            }
        }
}

// 降低背光函数
void AsusKeys::decrease_keyboard_backlight() {
    
}
