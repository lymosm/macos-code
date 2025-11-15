//
//  QCAInjector.c
//  QCAInjector
//
//  Created by lymos on 2025/10/9.
//

#include <mach/mach_types.h>

kern_return_t QCAInjector_start(kmod_info_t * ki, void *d);
kern_return_t QCAInjector_stop(kmod_info_t *ki, void *d);

kern_return_t QCAInjector_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t QCAInjector_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
