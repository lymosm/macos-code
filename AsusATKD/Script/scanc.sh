#!/bin/sh
clang -o scan_brightness_consumer scan_brightness_consumer.c -framework ApplicationServices
sudo ./scan_brightness_consumer
