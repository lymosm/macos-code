/* add your code here */
#include <IOKit/IOLib.h>
#include <IOKit/usb/IOUSBHostInterface.h>
#include "MyFirstUSBDriver.hpp"

OSDefineMetaClassAndStructors(lymosUSBDriver, IOService)
#define super IOService

void logEndpoint(IOUSBHostPipe* pipe){
    
  //  IOLog("lymosdev maxPacketsize: %d interval: %d", pipe->GetMaxPacketSize(), pipe->GetInterval());
}

bool lymosUSBDriver::init(OSDictionary* propTable){
    IOLog("lymosdev usb init(%p)\n", this);
    return super::init(propTable);
}

IOService* lymosUSBDriver::probe(IOService* provider, SInt32* score){
    IOLog("lymosdev usb probe\n");
    return super::probe(provider, score);
}

bool lymosUSBDriver::attach(IOService* provider){
    IOLog("lymosdev usb attach\n");
    return super::attach(provider);
}

void lymosUSBDriver::detach(IOService* provider){
    IOLog("lymosdev usb detach\n");
    super::detach(provider);
}

bool lymosUSBDriver::start(IOService* provider){
    IOLog("lymosdev usb start\n");
    IOUSBHostInterface* interface;
    // IOUSBFindEndpointRequest request;
    IOUSBHostPipe* pipe = NULL;
    
    
    if(! super::start(provider)){
        return false;
    }
    
    interface = OSDynamicCast(IOUSBHostInterface, provider);
    if(interface == NULL){
        IOLog("lymosdev usb start interface return null\n");
        return false;
    }
    
    // pipe = interface->FindNextPipe(NULL);
    if(pipe){
        logEndpoint(pipe);
        pipe->release();
    }
    return true;
}

void lymosUSBDriver::stop(IOService* provider){
    IOLog("lymosdev usb stop\n");
    super::stop(provider);
}

bool lymosUSBDriver::terminate(IOOptionBits options){
    IOLog("lymosdev usb terminate\n");
    return super::terminate(options);
}
