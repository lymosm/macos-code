/* add your code here */
#include "IOKitTest.hpp"
#include "IOKit/IOLib.h"

#define super IOService

OSDefineMetaClassAndStructors(lymosIOKitTest, IOService)

bool lymosIOKitTest::init(OSDictionary* dict){
    printf("lymosdev-lymosIOKitTest init\n");
    IOLog("lymosdev-lymosIOKitTest init\n");
    bool res = super::init(dict);
    return res;
}

bool lymosIOKitTest::start(IOService* provider){
    printf("lymosdev-lymosIOKitTest start\n");
    IOLog("lymosdev-lymosIOKitTest start\n");
    bool res = super::start(provider);
    return res;
}

void lymosIOKitTest::stop(IOService* provider){
    printf("lymosdev-lymosIOKitTest stop\n");
    IOLog("lymosdev-lymosIOKitTest stop\n");
    super::stop(provider);
}

void lymosIOKitTest::free(void){
    printf("lymosdev-lymosIOKitTest free\n");
    IOLog("lymosdev-lymosIOKitTest free\n");
    super::free();
}

IOService* lymosIOKitTest::probe(IOService* provider, SInt32* score){
    printf("lymosdev-lymosIOKitTest probe\n");
    IOLog("lymosdev-lymosIOKitTest probe\n");
    IOService *res = super::probe(provider, score);
    return res;
}
