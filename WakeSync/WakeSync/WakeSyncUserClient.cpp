//
//  WakeSyncUserClient.cpp
//  WakeSync
//
//  Created by lymos on 2025/8/17.
//
#include "WakeSyncUserClient.hpp"
#include "WakeSync.hpp"
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_version.hpp>

OSDefineMetaClassAndStructors(WakeSyncUserClient, IOUserClient)

bool WakeSyncUserClient::initWithTask(task_t owningTask, void* securityID, UInt32 type, OSDictionary* props) {
    DBGLOG("tommy", "initWithTask type=%u, pid=%d", type, proc_selfpid());

    if (!IOUserClient::initWithTask(owningTask, securityID, type, props)) return false;
    fClientTask = owningTask;
    return true;
}

bool WakeSyncUserClient::start(IOService* provider) {
    if (!IOUserClient::start(provider)) return false;
    fProvider = OSDynamicCast(WakeSync, provider);
    if (!fProvider){
        DBGLOG("tommy", " %s WakeSyncUserClient register failed", provider->getName());
        return false;
    }
    DBGLOG("tommy", " %s WakeSyncUserClient init success", provider->getName());
    // 建立一个共享队列（4KB 足够）
    
    fQueue = IOSharedDataQueue::withCapacity(64 * 1024);
    if (!fQueue) return false;

    fQueueMem = fQueue->getMemoryDescriptor();
    if (!fQueueMem) return false;
    fQueueMem->retain();
    // ✨ 注册 UserClient 服务
        registerService();
        DBGLOG("tommy", " %s WakeSyncUserClient registered in IORegistry", provider->getName());

    return true;
}

void WakeSyncUserClient::stop(IOService* provider) {
    if (fQueue) {
        fQueue->setNotificationPort(MACH_PORT_NULL);
    }
    if (fQueueMem) OSSafeReleaseNULL(fQueueMem);
    OSSafeReleaseNULL(fQueue);

    IOUserClient::stop(provider);
}

IOReturn WakeSyncUserClient::clientClose() {
    terminate();
    return kIOReturnSuccess;
}

IOReturn WakeSyncUserClient::clientDied() {
    return clientClose();
}

IOReturn WakeSyncUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 /*refCon*/) {
    DBGLOG("tommy", "registerNotificationPort: port=%p, type=%u", port, type);
    fNotifyPort = port;
    if (fQueue) {
        fQueue->setNotificationPort(port);
        DBGLOG("tommy", "registerNotificationPort: fQueue->setNotificationPort called");
    }
    return kIOReturnSuccess;
}


IOReturn WakeSyncUserClient::clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) {
    if (type != 0 || !fQueueMem) return kIOReturnBadArgument;
    *options = 0;
    fQueueMem->retain();
    *memory = fQueueMem;
    return kIOReturnSuccess;
}


void WakeSyncUserClient::postEvent(uint64_t event) {
    DBGLOG("tommy", "WakeSyncUserClient::postEvent called for event %llu", event);
    if (!fQueue) {
        DBGLOG("tommy", "postEvent: dataQueue is NULL, cannot enqueue event %llu", event);
        return; 
    }
    

    int maxTries = 10;
    bool ret = false;

    while (maxTries-- > 0) {
        ret = fQueue->enqueue(&event, sizeof(event));
        if (ret == true){
            DBGLOG("tommy", "postEvent: enqueued event %llu success", event);
            break;           // 成功，退出
        }
        

        // 队列满，丢弃最旧事件
        uint64_t dummy;
        uint32_t size = sizeof(dummy);
        if (fQueue->dequeue(&dummy, &size) != true) {
            DBGLOG("tommy", "postEvent: dequeue failed while freeing space");
            break;
        }
        DBGLOG("tommy", "postEvent: queue full, dropped oldest event %llu", dummy);
    }

    if (ret != true) {
        DBGLOG("tommy", "postEvent: enqueue ultimately failed %x for event %llu", ret, event);
    }
    
}


