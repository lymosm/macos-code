
#!/bin/sh
clang fn-brightness-daemon.c -o fn-brightness-daemon \
    -framework IOKit -framework CoreFoundation -framework ApplicationServices

sudo ./fn-brightness-daemon --force-keymap
