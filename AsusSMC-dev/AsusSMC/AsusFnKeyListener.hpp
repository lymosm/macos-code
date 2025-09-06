//
//  AsusFnKeyListener.hpp
//  AsusSMC
//
//  Created by lymos on 2025/7/20.
//  Copyright © 2025 Le Bao Hiep. All rights reserved.
//
#ifndef _ASUS_FN_KEY_LISTENER_H
#define _ASUS_FN_KEY_LISTENER_H

#include <IOKit/hidsystem/IOHIKeyboard.h>

class AsusFnKeyListener : public IOHIKeyboard {
    OSDeclareDefaultStructors(AsusFnKeyListener)

public:
    virtual void dispatchKeyboardEvent(unsigned int keyCode, bool goingDown, AbsoluteTime timeStamp) override;
};

#endif

