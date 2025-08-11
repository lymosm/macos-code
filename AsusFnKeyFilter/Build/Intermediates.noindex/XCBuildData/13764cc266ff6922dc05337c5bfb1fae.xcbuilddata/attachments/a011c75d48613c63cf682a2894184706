#include <mach/mach_types.h>
 
extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
__private_extern__ kern_return_t AsusFnKeyFilter_start(kmod_info_t *ki, void *data);
__private_extern__ kern_return_t AsusFnKeyFilter_stop(kmod_info_t *ki, void *data);
 
__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(lymos.AsusFnKeyFilter, "1.0.0d1", _start, _stop)
__private_extern__ kmod_start_func_t *_realmain = AsusFnKeyFilter_start;
__private_extern__ kmod_stop_func_t *_antimain = AsusFnKeyFilter_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__ ;
