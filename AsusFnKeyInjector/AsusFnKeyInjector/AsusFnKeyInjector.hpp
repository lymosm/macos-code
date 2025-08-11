#ifndef AsusFnKeyInjector_hpp
#define AsusFnKeyInjector_hpp

#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

class AsusFnKeyInjector : public IOHIKeyboard {
    OSDeclareDefaultStructors(AsusFnKeyInjector);
    
public:
    // IOHIKeyboard 方法
    virtual bool init(OSDictionary *properties) override;
    virtual void free() override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    
    // 键盘事件处理 - 使用3个参数的非重写版本
    virtual void dispatchKeyboardEvent(unsigned eventType,
                                    unsigned flags,
                                    unsigned keyCode);
    
private:
    // 背光控制
    void adjustKeyboardBacklight(int delta);
    UInt32 getCurrentBacklightLevel();
    void setBacklightLevel(UInt32 level);
    
    // ACPI 设备
    IOACPIPlatformDevice *acpiDevice;
    UInt32 maxBacklightLevel;
};

#endif /* AsusFnKeyInjector_hpp */
