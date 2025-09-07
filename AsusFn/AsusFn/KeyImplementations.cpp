//
//  KeyImplementations.cpp
//
//

#include "KeyImplementations.hpp"
#include "AsusFn.hpp"

// 真正定义
uint16_t lastBacklightValue = 0;

SMC_RESULT SMCALSValue::readAccess() {
   // DBGLOG("tommydebug", " SMCALSValue::readAccess");
    auto value = reinterpret_cast<Value *>(data);
    uint32_t lux = atomic_load_explicit(currentLux, memory_order_acquire);
    uint8_t bits = forceBits->bits();

    if (lux == 0xFFFFFFFF) {
        value->valid = false;
    } else {
        value->valid = true;
        if (!(bits & ALSForceBits::kALSForceHighGain))
            value->highGain = true;
        if (!(bits & ALSForceBits::kALSForceChan))
            value->chan0 = OSSwapHostToBigInt16(lux);
        if (!(bits & ALSForceBits::kALSForceLux))
            value->roomLux = OSSwapHostToBigInt32(lux << 14);
    }

    return SmcSuccess;
}

SMC_RESULT SMCKBrdBLightValue::update(const SMC_DATA *src)  {
    DBGLOG("tommydebug", " SMCKBrdBLightValue::update");
    lkb *value = new lkb;
    lilu_os_memcpy(value, src, size);

    // tval is in range [0x0, 0xffb]
    uint16_t tval = (value->val1 << 4) | (value->val2 >> 4);
    DBGLOG("tommydebug", " LKSB update %d", tval);
    lastBacklightValue = tval;
    delete value;

    if (asusFnInstance) {
        DBGLOG("tommydebug", " SMCKBrdBLightValue::update kSetKeyboardBacklightMessage");
        asusFnInstance->message(kSetKeyboardBacklightMessage, nullptr, &tval);
    }

    lilu_os_memcpy(data, src, size);
    return SmcSuccess;
}

SMC_RESULT F0Ac::readAccess() {
   // DBGLOG("tommydebug", " F0Ac::readAccess");
    uint16_t speed = atomic_load_explicit(currentSpeed, memory_order_acquire);
    *reinterpret_cast<uint16_t *>(data) = VirtualSMCAPI::encodeIntFp(SmcKeyTypeFpe2, speed);
    return SmcSuccess;
}

SMC_RESULT BDVT::update(const SMC_DATA *src)  {
    DBGLOG("tommydebug", " BDVT::update");
    bool state = false;
    lilu_os_memcpy(&state, src, size);

    // BDVT is 00 when battery health is enabled and 01 when disabled
    state = !state;

    AsusFn *drv = OSDynamicCast(AsusFn, dst);
    drv->toggleBatteryConservativeMode(state);

    lilu_os_memcpy(data, src, size);
    return SmcSuccess;
}
