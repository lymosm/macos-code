#!/bin/sh
clang smc.c main.c -framework IOKit -framework CoreFoundation -o smctest
sudo ./smctest
