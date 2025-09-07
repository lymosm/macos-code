#!/bin/bash
set -e

PLIST="/Library/LaunchAgents/tommy.asus.fnkeys.plist"
BIN="/usr/local/bin/fnkeys"

# 卸载 agent（尝试对当前控制台用户）
USER_UID=$(stat -f "%u" /dev/console 2>/dev/null || echo "")
if [ -n "$USER_UID" ]; then
  /bin/launchctl bootout gui/$USER_UID "$PLIST" 2>/dev/null || true
fi

# 也尝试 system 路径下的 bootout（防护）
/bin/launchctl bootout system "$PLIST" 2>/dev/null || true

# 删除文件
rm -f "$BIN"
rm -f "$PLIST"

# 忘记安装收据
/usr/sbin/pkgutil --forget tommy.asus.fnkeys 2>/dev/null || true

echo "[INFO] Asus Fn Key Daemon uninstalled"
exit 0
