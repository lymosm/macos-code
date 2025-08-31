
#!/bin/sh
clang keys.c -o keys \
    -framework IOKit -framework CoreFoundation -framework ApplicationServices

sudo ./keys
