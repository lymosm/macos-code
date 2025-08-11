#ifndef _ASUS_FN_KEY_FILTER_HPP
#define _ASUS_FN_KEY_FILTER_HPP

#include <IOKit/IOService.h>
#include <IOKit/hid/IOHIDEvent.h>

class IOHIDService : public IOService {
    OSDeclareAbstractStructors(IOHIDService)
    // 这里只做 forward 声明，实际代码中不会直接访问成员
};

class IOHIDEventServiceFilter : public OSObject {
    OSDeclareAbstractStructors(IOHIDEventServiceFilter)

public:
    virtual bool filterService(IOHIDService *service) = 0;
    virtual IOHIDEvent* filterEvent(IOHIDEvent *event, IOHIDService *service) = 0;
};

class AsusFnKeyFilter : public IOHIDEventServiceFilter {
    OSDeclareDefaultStructors(AsusFnKeyFilter)

public:
    virtual bool init(OSDictionary *dictionary = 0) override;
    virtual void free() override;

    virtual bool filterService(IOHIDService *service) override;
    virtual IOHIDEvent *filterEvent(IOHIDEvent *event, IOHIDService *service) override;
};

#endif

