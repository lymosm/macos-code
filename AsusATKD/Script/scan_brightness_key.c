#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <ApplicationServices/ApplicationServices.h>

// 发送单个键
static void send_key(uint16_t keycode) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, false);
    if (down && up) {
        CGEventPost(kCGHIDEventTap, down);
        CGEventPost(kCGHIDEventTap, up);
        printf("tommydebug: sent keycode=0x%x\n", keycode);
    }
    if (down) CFRelease(down);
    if (up) CFRelease(up);
}

// 忽略终止信号，但保留 SIGHUP（终端关闭可退出）
static void ignore_signals() {
    signal(SIGINT, SIG_IGN);   // Ctrl+C
    signal(SIGQUIT, SIG_IGN);  // Ctrl+\
    signal(SIGTERM, SIG_IGN);  // kill
    // SIGHUP 保持默认，不忽略
}

int main() {
    ignore_signals();
    printf("tommydebug: starting auto keycode scan for brightness down...\n");
    printf("tommydebug: watch for OSD, Ctrl+Z可挂起，关闭终端可退出\n");

    for (uint16_t keycode = 0; keycode <= 255; keycode++) {
        send_key(keycode);
        sleep(1);  // 每1秒尝试一个 keycode
    }

    printf("tommydebug: scan finished\n");
    return 0;
}
