#!/bin/sh
clang++ -std=c++11 exec-acpi.cpp -framework IOKit -framework CoreFoundation -o exec-acpi


sudo ./exec-acpi
