#include <mach/mach_types.h>

kern_return_t AsusFnKeyInjector_start(kmod_info_t * ki, void *d);
kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d);

kern_return_t AsusFnKeyInjector_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t AsusFnKeyInjector_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}


