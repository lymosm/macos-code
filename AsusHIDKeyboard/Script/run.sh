
#!/bin/sh
clang fnkeys.c -o fnkeys \
    -framework IOKit -framework CoreFoundation -framework ApplicationServices

sudo ./fnkeys --force-keymap
