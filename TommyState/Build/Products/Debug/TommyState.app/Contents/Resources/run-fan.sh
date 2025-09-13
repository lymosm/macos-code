#!/bin/sh
clang fan.c -framework IOKit -framework CoreFoundation -o fan
sudo ./fan
