#!/bin/bash
set -e

### active-audio
PLIST="/Library/LaunchAgents/tommy.active-audio.plist"
BIN="/usr/local/bin/active-audio"

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
/usr/sbin/pkgutil --forget tommy.active-audio 2>/dev/null || true


### sync-time
PLIST="/Library/LaunchDaemons/tommy.sync-time.plist"
BIN="/usr/local/bin/sync-time"

echo "==> 停止并卸载 LaunchDaemon"
if launchctl print system | grep -q com.apple.launchd; then
  launchctl bootout system "$PLIST" >/dev/null 2>&1 || true
else
  launchctl unload "$PLIST" >/dev/null 2>&1 || true
fi

# 删除文件
rm -f "$BIN"
rm -f "$PLIST"

# 忘记安装收据
/usr/sbin/pkgutil --forget tommy.sync-time 2>/dev/null || true

echo "[INFO] active-audio and sync-time Daemon uninstalled"
exit 0
