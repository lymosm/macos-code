//
//  LymosTimerUserClient.cpp
//  LymosTimer
//
//  Created by lymos on 2025/8/17.
//
#include "LymosTimerUserClient.hpp"
#include "LymosTimer.hpp"
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_version.hpp>

OSDefineMetaClassAndStructors(LymosTimerUserClient, IOUserClient)

bool LymosTimerUserClient::initWithTask(task_t owningTask, void* securityID, UInt32 type, OSDictionary* props) {
    DBGLOG("lymosdebug", "initWithTask type=%u, pid=%d", type, proc_selfpid());

    if (!IOUserClient::initWithTask(owningTask, securityID, type, props)) return false;
    fClientTask = owningTask;
    return true;
}

bool LymosTimerUserClient::start(IOService* provider) {
    if (!IOUserClient::start(provider)) return false;
    fProvider = OSDynamicCast(LymosTimer, provider);
    if (!fProvider){
        DBGLOG("lymosdebug", " %s LymosTimerUserClient register failed", provider->getName());
        return false;
    }
    DBGLOG("lymosdebug", " %s LymosTimerUserClient init success", provider->getName());
    // 建立一个共享队列（4KB 足够）
    
    fQueue = IOSharedDataQueue::withCapacity(64 * 1024);
    if (!fQueue) return false;

    fQueueMem = fQueue->getMemoryDescriptor();
    if (!fQueueMem) return false;
    fQueueMem->retain();
    // ✨ 注册 UserClient 服务
        registerService();
        DBGLOG("lymosdebug", " %s LymosTimerUserClient registered in IORegistry", provider->getName());

    return true;
}

void LymosTimerUserClient::stop(IOService* provider) {
    if (fQueue) {
        fQueue->setNotificationPort(MACH_PORT_NULL);
    }
    if (fQueueMem) OSSafeReleaseNULL(fQueueMem);
    OSSafeReleaseNULL(fQueue);

    IOUserClient::stop(provider);
}

IOReturn LymosTimerUserClient::clientClose() {
    terminate();
    return kIOReturnSuccess;
}

IOReturn LymosTimerUserClient::clientDied() {
    return clientClose();
}

IOReturn LymosTimerUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 /*refCon*/) {
    DBGLOG("lymosdebug", "registerNotificationPort: port=%p, type=%u", port, type);
    fNotifyPort = port;
    if (fQueue) {
        fQueue->setNotificationPort(port);
        DBGLOG("lymosdebug", "registerNotificationPort: fQueue->setNotificationPort called");
    }
    return kIOReturnSuccess;
}


IOReturn LymosTimerUserClient::clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) {
    if (type != 0 || !fQueueMem) return kIOReturnBadArgument;
    *options = 0;
    fQueueMem->retain();
    *memory = fQueueMem;
    return kIOReturnSuccess;
}


void LymosTimerUserClient::postEvent(uint64_t event) {
    DBGLOG("lymosdebug", "LymosTimerUserClient::postEvent called for event %llu", event);
    if (!fQueue) {
        DBGLOG("lymosdebug", "postEvent: dataQueue is NULL, cannot enqueue event %llu", event);
        return;
    }
    

    int maxTries = 10;
    bool ret = false;

    while (maxTries-- > 0) {
        ret = fQueue->enqueue(&event, sizeof(event));
        if (ret == true){
            DBGLOG("lymosdebug", "postEvent: enqueued event %llu success", event);
            break;           // 成功，退出
        }
        

        // 队列满，丢弃最旧事件
        uint64_t dummy;
        uint32_t size = sizeof(dummy);
        if (fQueue->dequeue(&dummy, &size) != true) {
            DBGLOG("lymosdebug", "postEvent: dequeue failed while freeing space");
            break;
        }
        DBGLOG("lymosdebug", "postEvent: queue full, dropped oldest event %llu", dummy);
    }

    if (ret != true) {
        DBGLOG("lymosdebug", "postEvent: enqueue ultimately failed %x for event %llu", ret, event);
    }
    
}


