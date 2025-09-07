//
//  osd.m
//  AsusKeys
//
//  Created by lymos on 2025/9/7.
//

#import <Cocoa/Cocoa.h>
#import <CoreWLAN/CoreWLAN.h>
#import <CoreServices/CoreServices.h>
#import <sys/ioctl.h>
#import <sys/socket.h>
#import <sys/kern_event.h>
#import <dlfcn.h>
#import "osd.h"          // Apple 自带的头文件，不能改
#import "osd_bridge.h"   // 我们自己写的桥接头
#import "BezelServices.h"
#include <IOKit/pwr_mgt/IOPMLib.h>


extern OSStatus MDSendAppleEventToSystemProcess(AEEventID eventToSend);

static void *(*_BSDoGraphicWithMeterAndTimeout)(CGDirectDisplayID arg0, BSGraphic arg1, int arg2, float v, int timeout) = NULL;

void osd_gotoSleep(void) {
    printf("tommydebug: osd_gotoSleep in\n");
    /*
    OSDManager *manager = [OSDManager sharedManager];
    [manager showImage:OSDGraphicSleep
           onDisplayID:CGMainDisplayID()
              priority:OSDPriorityDefault
         msecUntilFade:2000];
     */
    if (_BSDoGraphicWithMeterAndTimeout != NULL){ // El Capitan and probably older systems
        printf("tommydebug: osd_gotoSleep _BSDoGraphicWithMeterAndTimeout in\n");
        MDSendAppleEventToSystemProcess(kAESleep);
    }else {
        printf("tommydebug: osd_gotoSleep Sierra+\n");
        // Sierra+
        CGDirectDisplayID currentDisplayId = [NSScreen.mainScreen.deviceDescription [@"NSScreenNumber"] unsignedIntValue];
        [[NSClassFromString(@"OSDManager") sharedManager] showImage:OSDGraphicSleep onDisplayID:currentDisplayId priority:OSDPriorityDefault msecUntilFade:1000];
    }
    // 再调用 IOKit API 执行真正的睡眠
        io_connect_t port = IOPMFindPowerManagement(MACH_PORT_NULL);
        if (port) {
            IOPMSleepSystem(port);
            IOServiceClose(port);
        }
}

OSStatus MDSendAppleEventToSystemProcess(AEEventID eventToSendID) {
    AEAddressDesc targetDesc;
    static const ProcessSerialNumber kPSNOfSystemProcess = {0, kSystemProcess };
    AppleEvent eventReply = {typeNull, NULL};
    AppleEvent eventToSend = {typeNull, NULL};

    OSStatus status = AECreateDesc(typeProcessSerialNumber,
                                   &kPSNOfSystemProcess, sizeof(kPSNOfSystemProcess), &targetDesc);

    if (status != noErr) return status;

    status = AECreateAppleEvent(kCoreEventClass, eventToSendID,
                                &targetDesc, kAutoGenerateReturnID, kAnyTransactionID, &eventToSend);

    AEDisposeDesc(&targetDesc);

    if (status != noErr) return status;

    status = AESendMessage(&eventToSend, &eventReply,
                           kAENormalPriority, kAEDefaultTimeout);

    AEDisposeDesc(&eventToSend);
    if (status != noErr) return status;
    AEDisposeDesc(&eventReply);
    return status;
}
