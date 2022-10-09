//
//  HelloWorld.c
//  HelloWorld
//
//  Created by lymos on 2022/10/3.
//

#include <mach/mach_types.h>
#include <libkern/libkern.h>

kern_return_t HelloWorld_start(kmod_info_t * ki, void *d);
kern_return_t HelloWorld_stop(kmod_info_t *ki, void *d);

kern_return_t HelloWorld_start(kmod_info_t * ki, void *d)
{
    printf("HelloWorld coming\n");
    return KERN_SUCCESS;
}

kern_return_t HelloWorld_stop(kmod_info_t *ki, void *d)
{
    printf("GoodBye HelloWorld\n");
    return KERN_SUCCESS;
}
