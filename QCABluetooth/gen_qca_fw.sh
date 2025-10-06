#!/bin/bash
set -e

RAMPATCH="$1"
MAINFW="$2"
OUTPUT="$3"

echo '#pragma once' > "$OUTPUT"
echo '#include <stdint.h>' >> "$OUTPUT"
echo '#include <stddef.h>' >> "$OUTPUT"
echo '' >> "$OUTPUT"
echo '/* 自动生成的高通 AR3k 蓝牙固件数组 */' >> "$OUTPUT"
echo '' >> "$OUTPUT"

# rampatch
echo '/* rampatch firmware */' >> "$OUTPUT"
xxd -i "$RAMPATCH" | sed '1s/.*/const uint8_t qca_rampatch[] = {/' >> "$OUTPUT"
echo "const size_t qca_rampatch_len = $(stat -f%z "$RAMPATCH");" >> "$OUTPUT"
echo '' >> "$OUTPUT"

# main firmware
echo '/* main firmware */' >> "$OUTPUT"
xxd -i "$MAINFW" | sed '1s/.*/const uint8_t qca_mainfw[] = {/' >> "$OUTPUT"
echo "const size_t qca_mainfw_len = $(stat -f%z "$MAINFW");" >> "$OUTPUT"

