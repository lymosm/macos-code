#include <stdio.h>
#include <unistd.h>
#include <ApplicationServices/ApplicationServices.h>

#define CONSUMER_START 0x0C00
#define CONSUMER_END   0x0CFF

// 发送 Consumer usage
static void send_consumer_usage(uint16_t usage) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)usage, true);
    CGEventRef up   = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)usage, false);
    if (down && up) {
        CGEventPost(kCGHIDEventTap, down);
        CGEventPost(kCGHIDEventTap, up);
        printf("tommydebug: sent Consumer usage 0x%x\n", usage);
    }
    if (down) CFRelease(down);
    if (up) CFRelease(up);
}

int main() {
    printf("tommydebug: starting Consumer usage scan for brightness keys...\n");
    printf("tommydebug: watch for OSD, close terminal to stop\n");

    while (1) {
        for (uint16_t usage = CONSUMER_START; usage <= CONSUMER_END; usage++) {
            send_consumer_usage(usage);
            sleep(1); // 每秒尝试一个
        }
    }

    return 0;
}

