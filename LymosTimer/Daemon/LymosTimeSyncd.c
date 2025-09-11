// LymosTimeSyncd_msgloop.c
// 可靠通知接收：专用 mach_msg() 线程 + IODataQueueAllocateNotificationPort
//
// 编译：clang -framework IOKit -framework CoreFoundation -o LymosTimeSyncd LymosTimeSyncd_msgloop.c
// 运行：sudo ./LymosTimeSyncd

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IODataQueueClient.h>

#define SERVICE_NAME "LymosTimer"
#define WAIT_SECONDS 10

static io_connect_t g_conn = IO_OBJECT_NULL;
static IODataQueueMemory *g_queue = NULL;
static mach_vm_address_t g_queueAddr = 0;
static mach_vm_size_t g_queueSize = 0;

static mach_port_t g_notifyPort = MACH_PORT_NULL;
static pthread_t g_msgThread;
static volatile int g_running = 1;

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
            int ret = system("/usr/bin/sntp -sS ntp.aliyun.com");
            int ret2 = system("/usr/local/bin/active-audio");
            log_now("LymosTimeSyncd", "sntp returned %d", ret);
        }
    }
}

// 专用消息接收线程：阻塞在 mach_msg()，有消息就 drain_queue()
// 改进：在超时分支也主动 drain_queue()，作为对丢失通知的兜底措施。
static void* msg_loop(void* arg) {
    (void)arg;

    // mach_msg buffer：用联合体保证对齐
    typedef union {
        mach_msg_header_t head;
        uint8_t bytes[512]; // 扩大一些以防消息变大
    } msg_buf_t;

    msg_buf_t msg;
    const mach_msg_timeout_t timeout_ms = 10000; // 超时 2000 ms，超时时会主动 drain

    while (g_running) {
        // 清零 buffer
        memset(&msg, 0, sizeof(msg));

        kern_return_t kr = mach_msg(
            &msg.head,
            MACH_RCV_MSG | MACH_RCV_TIMEOUT,
            0,
            sizeof(msg),
            g_notifyPort,
            /*timeout_ms*/ timeout_ms,
            MACH_PORT_NULL
        );

        if (kr == KERN_SUCCESS) {
            // 收到任意消息都去尝试 drain（IODataQueue 的通知是“队列可读”的信号）
            log_now("MsgLoop", "mach_msg received -> draining queue");
            drain_queue();
        } else if (kr == MACH_RCV_TIMED_OUT) {
            // 超时作为备份：主动 drain 一次，防止在睡眠/唤醒期间丢失通知导致队列堆积
            // log_now("MsgLoop", "mach_msg timed out -> backup drain");
            drain_queue();
            continue;
        } else if (kr == MACH_RCV_INVALID_NAME || kr == MACH_RCV_PORT_DIED) {
            log_now("MsgLoop", "mach_msg recv port invalid/died: 0x%x", kr);
            break;
        } else {
            log_now("MsgLoop", "mach_msg recv error: 0x%x -- will attempt to drain and continue", kr);
            // 出错但端口未死，尝试一次 drain 后继续
            drain_queue();
            // 轻微休眠避免忙循环
            usleep(100 * 1000);
        }
    }
    log_now("MsgLoop", "exiting msg loop");
    return NULL;
}

static void cleanup(void) {
    log_now("LymosTimeSyncd", "cleanup called");
    g_running = 0;

    // 等接收线程退出
    if (g_msgThread) {
        pthread_join(g_msgThread, NULL);
        g_msgThread = 0;
    }

    if (g_queueAddr && g_conn != IO_OBJECT_NULL) {
        IOConnectUnmapMemory64(g_conn, 0, mach_task_self(), g_queueAddr);
        g_queueAddr = 0;
        g_queue = NULL;
    }
    if (g_conn != IO_OBJECT_NULL) {
        IOServiceClose(g_conn);
        g_conn = IO_OBJECT_NULL;
    }
    if (g_notifyPort != MACH_PORT_NULL) {
        mach_port_deallocate(mach_task_self(), g_notifyPort);
        g_notifyPort = MACH_PORT_NULL;
    }
}

// 信号处理：收到 SIGINT/SIGTERM 时干净退出
static void sig_handler(int sig) {
    (void)sig;
    log_now("LymosTimeSyncd", "signal received, shutting down...");
    g_running = 0;
    // wake 主线程（pause 会被信号唤醒），接收线程会在下一次 mach_msg 超时或收到消息时退出
}

int main(void) {
    if (geteuid() != 0) {
        fprintf(stderr, "LymosTimeSyncd: must run as root!\n");
        return 1;
    }

    atexit(cleanup);

    // 安装信号处理
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

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

    // 映射共享队列
    kr = IOConnectMapMemory64(g_conn, 0, mach_task_self(), &g_queueAddr, &g_queueSize, kIOMapAnywhere);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "IOConnectMapMemory64 failed: 0x%x", kr);
        return 1;
    }
    g_queue = (IODataQueueMemory *)g_queueAddr;
    log_now("LymosTimeSyncd", "queue mapped at %p, size=%llu",
            (void*)g_queueAddr, (unsigned long long)g_queueSize);

    // 关键：使用 IODataQueue 自带的 helper 生成通知端口
    g_notifyPort = IODataQueueAllocateNotificationPort();
    if (g_notifyPort == MACH_PORT_NULL) {
        log_now("LymosTimeSyncd", "IODataQueueAllocateNotificationPort failed");
        return 1;
    }
    log_now("LymosTimeSyncd", "notifyPort=0x%x allocated (IODataQueue helper)", g_notifyPort);

    // 把通知端口交给内核（selector/type 用 0；若你的 UserClient 另有 type 约定，保持一致）
    kr = IOConnectSetNotificationPort(g_conn, g_notifyPort, 0, 0);
    if (kr != KERN_SUCCESS) {
        log_now("LymosTimeSyncd", "IOConnectSetNotificationPort failed: 0x%x", kr);
        return 1;
    }

    // 先清空一次残留事件
    drain_queue();

    // 起接收线程，在 mach_msg 上阻塞
    int perr = pthread_create(&g_msgThread, NULL, msg_loop, NULL);
    if (perr != 0) {
        log_now("LymosTimeSyncd", "pthread_create failed: %d", perr);
        return 1;
    }

    log_now("LymosTimeSyncd", "message loop thread started; press Ctrl+C to quit");
    // 主线程闲置。收到 SIGINT/SIGTERM 后会退出（pause 会被信号中断）
    while (g_running) pause();

    log_now("LymosTimeSyncd", "main exiting");
    return 0;
}

