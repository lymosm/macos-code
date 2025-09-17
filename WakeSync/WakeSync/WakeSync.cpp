#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_version.hpp>
#include "WakeSync.hpp"
#include <sys/proc.h>
#include <sys/systm.h>
#include <libkern/libkern.h>
#include <kern/task.h>
#include <kern/thread.h>


bool ADDPR(debugEnabled) = false;
uint32_t ADDPR(debugPrintDelay) = 0;

#define kDeliverNotifications   "RM,deliverNotifications"


#define super IOService

OSDefineMetaClassAndStructors(WakeSync, super)


// newUserClient: 允许一个用户态连接
IOReturn WakeSync::newUserClient(task_t owningTask, void* securityID, UInt32 type,
                                   OSDictionary* properties, IOUserClient** handler)
{
    DBGLOG("tommy", " WakeSync::newUserClient");
    
    if (!_userClients) {
        _userClients = OSSet::withCapacity(2); // 初始容量可调
    }

    auto uc = OSTypeAlloc(WakeSyncUserClient);
    if (!uc) return kIOReturnNoMemory;

    if (!uc->initWithTask(owningTask, securityID, type, properties) ||
        !uc->attach(this) || !uc->start(this)) {
        uc->release();
        return kIOReturnInternalError;
    }

    // 加入集合
    _userClients->setObject(uc);
    *handler = uc;
    return kIOReturnSuccess;

}

bool WakeSync::start(IOService *provider) {
    if (!super::start(provider))
        return false;

    setProperty("VersionInfo", kextVersion);
    ADDPR(debugEnabled) = checkKernelArgument("-lytdbg") || checkKernelArgument("-liludbgall");
    //ADDPR(debugEnabled) = true;
    PE_parse_boot_argn("liludelay", &ADDPR(debugPrintDelay), sizeof(ADDPR(debugPrintDelay)));
    workLoop = IOWorkLoop::workLoop();
    commandGate = IOCommandGate::commandGate(this);
    if (!workLoop || !commandGate || (workLoop->addEventSource(commandGate) != kIOReturnSuccess)) {
        SYSLOG("tommy", "failed to add commandGate");
        return false;
    }
    // 使用显式 cast 避开 IOPMrootDomain 不可见的问题
    // ✨ 创建唤醒后延时定时器
        postWakeTimer = IOTimerEventSource::timerEventSource(this, &WakeSync::postWakeTimerFired);
        if (!postWakeTimer || workLoop->addEventSource(postWakeTimer) != kIOReturnSuccess) {
            SYSLOG("tommy", "failed to create/attach postWakeTimer");
            return false;
        }

        // ✨ 注册 PM rootDomain interest（你之前的实现可以替换成下面这段）
        IOService *rootDomain = reinterpret_cast<IOService*>(IOService::getPMRootDomain());
        if (rootDomain) {
            powerNotifier = rootDomain->registerInterest(gIOGeneralInterest, powerEventHandler, this, 0);
            if (!powerNotifier) {
                SYSLOG("tommy", "failed to register PM rootDomain interest");
            } else {
                DBGLOG("tommy", "PM rootDomain interest registered");
            }
        }
    
    
    registerService();

    return true;
}

void WakeSync::stop(IOService *provider) {
    
    if (postWakeTimer) {
            postWakeTimer->cancelTimeout();
            workLoop->removeEventSource(postWakeTimer);
            OSSafeReleaseNULL(postWakeTimer);
        }

        if (powerNotifier) {
            powerNotifier->remove();
            powerNotifier = nullptr;
        }
    
    if (_userClients) {
        OSCollectionIterator* iter = OSCollectionIterator::withCollection(_userClients);
        if (iter) {
            IOUserClient* client;
            while ((client = OSDynamicCast(IOUserClient, iter->getNextObject()))) {
                client->detach(this);
                client->release();
            }
            iter->release();
        }
        _userClients->flushCollection();
    }

    workLoop->removeEventSource(commandGate);
    OSSafeReleaseNULL(commandGate);
    OSSafeReleaseNULL(workLoop);

    super::stop(provider);
}

IOReturn WakeSync::powerEventHandler(void *target, void *refCon, UInt32 messageType,
                                       IOService *provider, void *messageArgument, vm_size_t argSize)
{
    // DBGLOG("tommy", "powerEventHandler");
    auto self = OSDynamicCast(WakeSync, reinterpret_cast<OSMetaClassBase*>(target));
    if (!self) {
        DBGLOG("tommy", "powerEventHandler: cannot cast target");
        return kIOReturnError;
    }

    switch (messageType) {
        case kIOMessageSystemWillSleep:
            DBGLOG("tommy", "System will sleep (kIOMessageSystemWillSleep)");
            break;

        case kIOMessageCanSystemSleep:
            DBGLOG("tommy", "System can sleep (kIOMessageCanSystemSleep)");
            break;

        case kIOMessageSystemHasPoweredOn:
            DBGLOG("tommy", "System has powered on (wake)");
            // ✨ 唤醒后 2 秒再做同步（可按需调 delay）
                        self->schedulePostWakeSync(1000);
            break;

        default:
           // DBGLOG("tommy", "Power event received type=0x%08x", messageType);
            break;
    }

    return kIOReturnSuccess;
}

void WakeSync::schedulePostWakeSync(uint32_t delayMs) {
    if (!postWakeTimer) return;
    // 如果短时间内多次唤醒/重复触发，先取消旧的
    postWakeTimer->cancelTimeout();
    postWakeTimer->setTimeoutMS(delayMs);
    DBGLOG("tommy", "post-wake time sync scheduled in %u ms", delayMs);
}

void WakeSync::postWakeTimerFired(OSObject *owner, IOTimerEventSource *sender) {
    auto self = OSDynamicCast(WakeSync, owner);
    if (!self || !self->commandGate) return;
    self->commandGate->runAction(
        OSMemberFunctionCast(IOCommandGate::Action, self, &WakeSync::performTimeSyncGated)
    );
}

void WakeSync::performTimeSyncGated() {
    // 记录唤醒时间（可用于诊断）
    uint64_t uptime = 0;
    clock_get_uptime(&uptime);
    setProperty("LastWakeUptime", (uint64_t)uptime, 64);

    clock_sec_t sec; clock_usec_t usec;
    clock_get_calendar_microtime(&sec, &usec);
    setProperty("LastWakeCalendarSec", (uint64_t)sec, 64);
    setProperty("LastWakeCalendarUSec", (uint64_t)usec, 64);

    // ✨ 调用可选的外部时间同步实现
        DBGLOG("tommy", "invoking LymosTimeSync_Perform()");
        LymosTimeSync_Perform();
        DBGLOG("tommy", "time sync complete");
    
}

void WakeSync::LymosTimeSync_Perform(void) {
    // 这里执行真正的时间同步：例如
    // - 重新从 RTC 读取并校准你的驱动内部计时
    // - 触发用户态 daemon 做 NTP 校准（通过 IOUserClient 通知）
    // - 重新对齐你的硬件/固件时基
    // 示例只做日志：
    // 通知用户态：1 = 需要进行网络时间同步
    /*
        if (userClient) {
            userClient->postEvent(1);
            DBGLOG("tommy", "posted wake event to user client");
        } else {
            DBGLOG("tommy", "no user client connected; skip userland sync");
        }
     */
    
    if (_userClients && _userClients->getCount() > 0) {
        OSCollectionIterator* i = OSCollectionIterator::withCollection(_userClients);
        if (i) {
            while (auto client = OSDynamicCast(WakeSyncUserClient, i->getNextObject())) {
                DBGLOG("tommy", "posted wake event to user client");
                client->postEvent(1);
            }
            i->release();
        }
    }

    DBGLOG("tommy", "perform time sync after wake\n");
}

