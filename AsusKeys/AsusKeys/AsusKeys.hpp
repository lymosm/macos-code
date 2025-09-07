// File: AsusKeys.hpp
#ifndef AsusKeys_hpp
#define AsusKeys_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h> // for IOUserClient*
#include <libkern/OSAtomic.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>

class AsusKeys : public IOService {
    OSDeclareDefaultStructors(AsusKeys)

public:
    virtual bool init(OSDictionary* dict) override;
    virtual void free() override;

    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    virtual IOReturn newUserClient(task_t owningTask,
                                   void* securityID,
                                   UInt32 type,
                                   OSDictionary* properties,
                                   IOUserClient** handler) override;

    // called from userclient when user-space sends key events
    void handleUserMessage(uint32_t usage, uint32_t page, int32_t pressed, uint32_t keycode);
    void decrease_keyboard_backlight(); // will be executed on command gate
    void increase_keyboard_backlight();
    void decrease_backlight(); // will be executed on command gate
    void increase_backlight();
    void close_backlight();
    void toggle_touchpad();

    IOReturn evaluateAcpiFromUser(const char* device, const char* method, OSArray* params, OSObject** outResult);


private:
    IOService* _provider = nullptr;
    IOWorkLoop* _workLoop = nullptr;
    IOCommandGate* _commandGate = nullptr;

    IONotifier* _acpiNotifier = nullptr;
    IOACPIPlatformDevice* _ec0Device = nullptr;
    IOACPIPlatformDevice* _atkDevice;   // 新增 ATKD 设备


    static bool acpiPublishHandler(void* target, void* refCon, IOService* newService, IONotifier* notifier);
    void onACPIDevicePublished(IOACPIPlatformDevice* dev);
};

#endif /* AsusKeys_hpp */



// File: Info.plist (snippet) - put this into your kext's plist
/*

*/
