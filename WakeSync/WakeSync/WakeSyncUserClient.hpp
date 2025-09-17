//
//  WakeSyncUserClient.hpp
//  WakeSync
//
//  Created by lymos on 2025/8/17.
//
#pragma once
#include <IOKit/IOUserClient.h>
#include <IOKit/IOSharedDataQueue.h>

class WakeSync; // 前向声明

class WakeSyncUserClient final : public IOUserClient {
    OSDeclareDefaultStructors(WakeSyncUserClient)

private:
    task_t                     fClientTask {nullptr};
    WakeSync*                fProvider {nullptr};
    IOSharedDataQueue*         fQueue {nullptr};
    IOMemoryDescriptor*        fQueueMem {nullptr};
    mach_port_t                fNotifyPort {MACH_PORT_NULL};

public:
    // IOService
    bool start(IOService* provider) APPLE_KEXT_OVERRIDE;
    void stop(IOService* provider) APPLE_KEXT_OVERRIDE;

    // IOUserClient
    bool initWithTask(task_t owningTask, void* securityID, UInt32 type, OSDictionary* properties) APPLE_KEXT_OVERRIDE;
    IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    IOReturn clientDied() APPLE_KEXT_OVERRIDE;

    // 让用户态设定通知端口（IOConnectSetNotificationPort 会走到这里）
    IOReturn registerNotificationPort(mach_port_t port,UInt32 type,UInt32 refCon) APPLE_KEXT_OVERRIDE;

    // 把共享内存（队列）暴露给用户态：type=0
    IOReturn clientMemoryForType(UInt32 type, IOOptionBits* options, IOMemoryDescriptor** memory) APPLE_KEXT_OVERRIDE;

    // 供 provider 调用：向队列里推一个事件（比如 1 = 需要校时）
    void postEvent(uint64_t code);
};

