#!/bin/sh
clang -o send_usage send_usage.c -framework IOKit -framework CoreFoundation
./send_usage 0xC5 0xff31
