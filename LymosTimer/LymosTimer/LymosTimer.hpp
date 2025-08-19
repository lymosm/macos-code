
#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include <IOKit/IOMessage.h>       // 为 kIOMessageSystemWillSleep 等常量
#include <IOKit/IOTimerEventSource.h> // 新增
#include "LymosTimerUserClient.hpp"


class IOPMrootDomain; // 前向声明

class LymosTimer : public IOService {
    OSDeclareDefaultStructors(LymosTimer)
private:
    // ✨ 唤醒后延时任务
    IOTimerEventSource *postWakeTimer {nullptr};

        // ✨ power/PM
        IONotifier *powerNotifier {nullptr};
        static IOReturn powerEventHandler(void *target, void *refCon, UInt32 messageType,
                                          IOService *provider, void *messageArgument, vm_size_t argSize);

        // ✨ 定时器回调 + 实际执行逻辑（在 gate 内）
        static void postWakeTimerFired(OSObject *owner, IOTimerEventSource *sender);
        void performTimeSyncGated();

        // ✨ 通过 commandGate 在 gate 中调用
        void schedulePostWakeSync(uint32_t delayMs = 2000);
    
    void LymosTimeSync_Perform(void);
    
    OSSet* _userClients = nullptr;   // 保存所有连接的 user client
    LymosTimerUserClient* userClient {nullptr};
    friend class LymosTimerUserClient; // 让 user client 能访问需要的接口
    IOWorkLoop *workLoop {nullptr};
    IOCommandGate *commandGate {nullptr};
    
public:
    
    virtual IOReturn newUserClient(task_t owningTask, void* securityID, UInt32 type,
                                   OSDictionary* properties, IOUserClient** handler) override;



    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    
};

