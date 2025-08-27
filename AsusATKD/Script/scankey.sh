#!/bin/sh
clang -o scan_brightness_key scan_brightness_key.c -framework ApplicationServices
sudo ./scan_brightness_key

