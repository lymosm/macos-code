//
//  test_driverkit.cpp
//  test-driverkit
//
//  Created by lymos on 2022/9/25.
//

#include <os/log.h>

#include <DriverKit/IOUserServer.h>
#include <DriverKit/IOLib.h>

#include "test_driverkit.h"

kern_return_t
IMPL(test_driverkit, Start)
{
    kern_return_t ret;
    ret = Start(provider, SUPERDISPATCH);
    os_log(OS_LOG_DEFAULT, "Hello World");
    return ret;
}
