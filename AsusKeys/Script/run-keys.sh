
#!/bin/sh
clang keys.c osd.m -o keys \
    -framework IOKit \
    -framework CoreFoundation \
    -framework ApplicationServices \
    -framework Cocoa
sudo ./keys
