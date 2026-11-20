#!/usr/bin/env bash
# culdcept-nx (2026-11-20): builds tools/switch/av1bench/out/av1bench.nro (main.c: the AV1 / H.265 decode benchmark) with
# devkitPro's libnx, dav1d and switch-ffmpeg 7.1 (with nvtegra). No game code: a standalone NRO.
# usage (Git Bash): tools/switch/av1bench/build.sh
set -eu
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OUT=$HERE/out
DKP=/c/devkitPro
BIN=$DKP/devkitA64/bin
PL=$DKP/portlibs/switch
mkdir -p "$OUT"
export DEVKITPRO=C:/devkitPro  # switch.specs finds switch.ld through it (Git Bash's own /opt/devkitpro is not there)
"$BIN/aarch64-none-elf-gcc" -O2 -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -D__SWITCH__ \
  -I"$PL/include" -I"$DKP/libnx/include" "$HERE/main.c" \
  -specs="$DKP/libnx/switch.specs" -L"$PL/lib" -L"$DKP/libnx/lib" \
  -lavcodec -lswresample -lavutil -ldav1d -lz -lnx -lm -o "$OUT/av1bench.elf"
"$DKP/tools/bin/nacptool" --create "AV1 decode benchmark" "culdcept-nx" "1.0" "$OUT/av1bench.nacp"
"$DKP/tools/bin/elf2nro" "$OUT/av1bench.elf" "$OUT/av1bench.nro" --nacp="$OUT/av1bench.nacp"
ls -la "$OUT/av1bench.nro"
