#ifndef AsusFnKeyInjector_hpp
#define AsusFnKeyInjector_hpp

#include <IOKit/hidsystem/IOHIKeyboard.h>

class AsusFnKeyInjector : public IOHIKeyboard {
    OSDeclareDefaultStructors(AsusFnKeyInjector)

public:
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // ❌ 不写 override！因为它不是虚函数
    virtual void dispatchKeyboardEvent(unsigned eventType, unsigned flags, unsigned keyCode);
};

#endif /* AsusFnKeyInjector_hpp */

