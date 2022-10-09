/* add your code here */
#include "iokit_test.hpp"

#define super IOService

OSDefineDefaultStructors(HelloWorld, IOService)

bool HelloWorld::init(OSDictionary* dict){
    bool ret = super::init(dict);
    IOLog("driver loading...");
    return ret;
}

void HelloWorld::free(void){
    IOLog("driver freeing...");
    super::free();
}

// 驱动正在匹配
IOService* HelloWorld::probe(IOService* provider, SInt32* score){
    IOService* myservice = super::probe(provider, score);
    IOLog("driver is probe ing...");
    return myservice;
}

bool HelloWorld::start(IOService* provider){
    bool ret = super::start(provider);
    IOLog("driver is starting");
    return ret;
}

void HelloWorld::stop(IOService* provider){
    super::stop(provider);
    IOLog("driver is stoping");
}
