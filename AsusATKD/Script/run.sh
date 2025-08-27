
#!/bin/sh
clang -o fn-brightness-daemon fn-brightness-daemon.c \
    -framework CoreFoundation -framework IOKit

sudo ./fn-brightness-daemon
