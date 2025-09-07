//
//  AsusFnKeyListener.cpp
//
//

#include "AsusFnKeyListener.hpp"
#include <IOKit/IOLib.h>

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(AsusFnKeyListener, IOHIKeyboard)

void AsusFnKeyListener::dispatchKeyboardEvent(unsigned int keyCode, bool goingDown, AbsoluteTime timeStamp) {
    if (goingDown) {
        if (keyCode == 0x91) {
        }
    }

    // 可选：继续传递事件到上层，调试阶段保留
    super::dispatchKeyboardEvent(keyCode, goingDown, timeStamp);
}
