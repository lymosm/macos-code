#!/bin/bash
set -e

### active-audio
# 安装后把 plist 放到 /Library/LaunchAgents（pkg 已经把文件放好了）
PLIST="/Library/LaunchAgents/tommy.active-audio.plist"

# 确保权限与属主
if [ -f "$PLIST" ]; then
  /usr/sbin/chown root:wheel "$PLIST" || true
  /bin/chmod 644 "$PLIST" || true
fi

# 确保二进制权限
if [ -f "/usr/local/bin/active-audio" ]; then
  /usr/sbin/chown root:wheel /usr/local/bin/active-audio || true
  /bin/chmod 755 /usr/local/bin/active-audio || true
fi

# 尝试在当前登录用户 session 下立即加载 agent（当 pkg 在 GUI 下以管理员身份安装时，这里能生效）
USER_UID=$(stat -f "%u" /dev/console 2>/dev/null || echo "")
if [ -n "$USER_UID" ]; then
  # 使用 bootstrap 以加载到当前用户 GUI session
  /bin/launchctl bootstrap gui/$USER_UID "$PLIST" 2>/dev/null || true
  /bin/launchctl kickstart -k "gui/$USER_UID/tommy.active-audio" 2>/dev/null || true
fi


### sync-time
# 安装后把 plist 放到 /Library/LaunchDaemons（pkg 已经把文件放好了）
PLIST="/Library/LaunchDaemons/tommy.sync-time.plist"

# 确保权限与属主
if [ -f "$PLIST" ]; then
  /usr/sbin/chown root:wheel "$PLIST" || true
  /bin/chmod 644 "$PLIST" || true
fi

# 确保二进制权限
if [ -f "/usr/local/bin/sync-time" ]; then
  /usr/sbin/chown root:wheel /usr/local/bin/sync-time || true
  /bin/chmod 755 /usr/local/bin/sync-time || true
fi

echo "==> 载入 LaunchDaemon"
# macOS 10.13+ 新接口
if launchctl print system >/dev/null 2>&1; then
  launchctl bootout system "$PLIST" >/dev/null 2>&1 || true
  launchctl bootstrap system "$PLIST"
  launchctl enable system/"$LABEL"
else
  # 旧系统 fallback
  launchctl unload "$PLIST" >/dev/null 2>&1 || true
  launchctl load -w "$PLIST"
fi

exit 0
