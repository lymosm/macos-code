//
//  AsusFnKeyFilter.cpp
//  AsusFnKeyFilter
//
//  Created by lymos on 2025/7/23.
//

#include "AsusFnKeyFilter.hpp"
#include <IOKit/IOLib.h>

#define super IOHIDEventServiceFilter
OSDefineMetaClassAndStructors(AsusFnKeyFilter, IOHIDEventServiceFilter)

bool AsusFnKeyFilter::init(OSDictionary *dict) {
    if (!super::init(dict)) {
        return false;
    }
    IOLog("lymoskey AsusFnKeyFilter: Initialized\n");
    return true;
}

void AsusFnKeyFilter::free() {
    IOLog("lymoskey AsusFnKeyFilter: Freed\n");
    super::free();
}

bool AsusFnKeyFilter::filterService(IOHIDService *service) {
    // 拦截所有 IOHIDService
    return true;
}

IOHIDEvent* AsusFnKeyFilter::filterEvent(IOHIDEvent *event, IOHIDService *service) {
    if (!event)
        return event;

    if (event->getType() == kIOHIDEventTypeKeyboard) {
        UInt32 usagePage = event->getIntegerValue(kIOHIDEventFieldKeyboardUsagePage);
        UInt32 usage = event->getIntegerValue(kIOHIDEventFieldKeyboardUsage);

        if (usagePage == 0x07 && usage == 0x91) {
            IOLog("lymoskey AsusFnKeyFilter: Fn+F6 detected! usage=0x%X\n", usage);

            // TODO: 实现亮度调节代码
        }
    }

    return event; // 继续传递
}
