#include <IOKit/IOLib.h>

extern "C" {
    kern_return_t AsusFnKeyInjector_start(kmod_info_t * ki, void *d);
    kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d);
}

kern_return_t AsusFnKeyInjector_start(kmod_info_t * ki, void *d) {
    IOLog("AsusFnKeyInjector: Kernel extension started\n");
    return KERN_SUCCESS;
}

kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d) {
    IOLog("AsusFnKeyInjector: Kernel extension stopped\n");
    return KERN_SUCCESS;
}

