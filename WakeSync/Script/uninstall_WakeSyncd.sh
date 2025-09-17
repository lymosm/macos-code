#!/bin/bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "请用 sudo 运行：sudo $0"
  exit 1
fi

BIN="/usr/local/bin/WakeSyncd"
PLIST="/Library/LaunchDaemons/com.tommy.wakesync.plist"
LABEL="com.tommy.wakesync"
LOG_OUT="/var/log/tommy.wakesync.out"
LOG_ERR="/var/log/tommy.wakesync.err"

echo "==> 停止并卸载 LaunchDaemon"
if launchctl print system | grep -q com.apple.launchd; then
  launchctl bootout system "$PLIST" >/dev/null 2>&1 || true
else
  launchctl unload "$PLIST" >/dev/null 2>&1 || true
fi

echo "==> 删除文件"
rm -f "$PLIST"
rm -f "$BIN"

# 日志可按需保留
# rm -f "$LOG_OUT" "$LOG_ERR"

echo "==> 卸载完成"

