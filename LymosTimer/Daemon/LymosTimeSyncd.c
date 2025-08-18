// LymosTimeSyncd.c — 调试版：每2秒心跳日志 + 打印关键变量
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IODataQueueClient.h>

#define SERVICE_NAME "LymosTimer"
#define WAIT_SECONDS 10

static io_connect_t g_conn = IO_OBJECT_NULL;
static IODataQueueMemory *g_queue = NULL;
static CFMachPortRef g_cfNotifyPort = NULL;
static CFRunLoopSourceRef g_notifySource = NULL;
static mach_port_t g_notifyPort = MACH_PORT_NULL;
static mach_vm_address_t g_queueAddr = 0;
static mach_vm_size_t g_queueSize = 0;

static void log_now(const char *tag, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    va_list ap; va_start(ap, fmt);
    fprintf(stdout, "[%s] %s: ", timeStr, tag);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

// 把队列里的事件全部取空
static void drain_queue(void) {
    if (!g_queue) return;
    while (IODataQueueDataAvailable(g_queue)) {
        uint64_t event = 0;
        uint32_t size = sizeof(event);
        kern_return_t kr = IODataQueueDequeue(g_queue, &event, &size);
        if (kr != KERN_SUCCESS || size != sizeof(event)) {
            log_now("LymosTimeSyncd", "Dequeue error: kr=0x%x size=%u", kr, size);
            break;
        }
        log_now("LymosTimeSyncd", "Dequeued event %llu", (unsigned long long)event);
        if (event == 1) {
            log_now("LymosTimeSyncd", "wake event -> syncing time");
            int ret = system("/usr/bin/sntp -sS time.apple.com");
            log_now("LymosTimeSyncd", "sntp returned %d", ret);
        }
    }
}

// 内核发来消息时的回调
static void queue_port_callback(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    (void)port; (void)msg; (void)size; (void)info;
    log_now("LymosTimeSyncd", "queue_port_callback fired, draining queue");
    drain_queue();
}

// 心跳定时器：每2秒打印一次调试信息
static void heartbeat_callback(CFRunLoopTimerRef timer, void *info) {
    (void)timer; (void)info;
    log_now("Heartbeat",
        "g_conn=0x%x g_queue=%p g_notifyPort=0x%x g_cfNotifyPort=%p queueAddr=%p queueSize=%llu",
        g_conn, g_queue, g_notifyPort, g_cfNotifyPort,
        (void*)g_queueAddr, (unsigned long long)g_queueSize);
    // 顺便尝试 drain（避免丢事件）
    drain_queue();
}

int main(void) {
    if (geteuid() != 0) {
        fprintf(stderr, "LymosTimeSyncd: must run as root!\n");
        return 1;
    }

    log_now("LymosTimeSyncd", "waiting for %s provider...", SERVICE_NAME);
    io_service_t service = IO_OBJECT_NULL;
    for (int i = 0; i < WAIT_SECONDS; ++i) {
        service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(SERVICE_NAME));
        if (service != IO_OBJECT_NULL) break;
        sleep(1);
    }
    if (service == IO_OBJECT_NULL) {
        log_now("LymosTimeSyncd", "%s provider not found after %d seconds", SERVICE_NAME, WAIT_SECONDS);
        return 1;
    }

    kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &g_conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "IOServiceOpen failed: 0x%x", kr);
        return 1;
    }
    log_now("LymosTimeSyncd", "opened provider and obtained connection (user client)");

    // 1) 创建通知端口
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &g_notifyPort);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "mach_port_allocate failed: 0x%x", kr);
        IOServiceClose(g_conn);
        return 1;
    }
    log_now("LymosTimeSyncd", "notifyPort=0x%x allocated", g_notifyPort);

    // 2) 把通知端口告诉内核
    kr = IOConnectSetNotificationPort(g_conn, g_notifyPort, 0, 0);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "IOConnectSetNotificationPort failed: 0x%x", kr);
        mach_port_deallocate(mach_task_self(), g_notifyPort);
        IOServiceClose(g_conn);
        return 1;
    }

    // 3) 映射共享队列内存
    kr = IOConnectMapMemory64(g_conn, 0, mach_task_self(), &g_queueAddr, &g_queueSize, kIOMapAnywhere);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "IOConnectMapMemory64 failed: 0x%x", kr);
        mach_port_deallocate(mach_task_self(), g_notifyPort);
        IOServiceClose(g_conn);
        return 1;
    }
    g_queue = (IODataQueueMemory *)g_queueAddr;
    log_now("LymosTimeSyncd", "queue mapped at %p, size=%llu", (void*)g_queueAddr, (unsigned long long)g_queueSize);

    // 清残留事件
    drain_queue();

    // 4) CFMachPort 包装通知端口
    CFMachPortContext ctx = {0};
    g_cfNotifyPort = CFMachPortCreateWithPort(kCFAllocatorDefault, g_notifyPort, queue_port_callback, &ctx, false);
    if (!g_cfNotifyPort) {
        log_now("LymosTimeSyncd", "CFMachPortCreateWithPort failed");
        IOConnectUnmapMemory64(g_conn, 0, mach_task_self(), g_queueAddr);
        mach_port_deallocate(mach_task_self(), g_notifyPort);
        IOServiceClose(g_conn);
        return 1;
    }
    g_notifySource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_cfNotifyPort, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), g_notifySource, kCFRunLoopDefaultMode);

    // 5) 加心跳定时器（每2秒触发一次）
    CFRunLoopTimerContext tctx = {0};
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
                                CFAbsoluteTimeGetCurrent() + 2.0,
                                2.0, 0, 0,
                                heartbeat_callback, &tctx);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);

    log_now("LymosTimeSyncd", "entering CFRunLoop with heartbeat...");
    CFRunLoopRun();

    return 0;
}
