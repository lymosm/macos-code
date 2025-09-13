#!/bin/sh
clang superio.c -framework IOKit -framework CoreFoundation -o superio
./superio
