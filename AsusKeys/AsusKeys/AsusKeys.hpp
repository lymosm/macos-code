// AsusKeys.hpp
#ifndef AsusKeys_hpp
#define AsusKeys_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h> // for IOUserClient*


class AsusKeys : public IOService {
    OSDeclareDefaultStructors(AsusKeys)

public:
    virtual bool init(OSDictionary* dict) override;
    virtual void free() override;

    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // 新增：创建 user client（由 IOServiceOpen 触发）
    virtual IOReturn newUserClient(task_t owningTask,
                                   void* securityID,
                                   UInt32 type,
                                   OSDictionary* properties,
                                   IOUserClient** handler) override;

    // 用户态消息到来时调用（由 UserClient 调用）
    void handleUserMessage(uint32_t usage, uint32_t page, int32_t pressed, uint32_t keycode);
    
    void decrease_keyboard_backlight();

private:
    IOService* _provider;
};

#endif /* AsusKeys_hpp */
