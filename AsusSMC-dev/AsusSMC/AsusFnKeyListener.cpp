//
//  AsusFnKeyListener.cpp
//  AsusSMC
//
//  Created by lymos on 2025/7/20.
//  Copyright © 2025 Le Bao Hiep. All rights reserved.
//

#include "AsusFnKeyListener.hpp"
#include <IOKit/IOLib.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AsusFnKeyListener, IOHIKeyboard)

void AsusFnKeyListener::dispatchKeyboardEvent(unsigned int keyCode, bool goingDown, AbsoluteTime timeStamp) {
    IOLog("lymoskey AsusSMC [FnKey] Fn+F6 detected (keyCode=0x%X)\n", keyCode);
    if (goingDown) {
        if (keyCode == 0x91) {
            IOLog("lymoskey AsusSMC [FnKey] Fn+F6 detected (keyCode=0x%X)\n", keyCode);
        }
    }

    // 可选：继续传递事件到上层，调试阶段保留
    super::dispatchKeyboardEvent(keyCode, goingDown, timeStamp);
}
