#!/bin/bash
set -euo pipefail

# 需要 root
if [[ $EUID -ne 0 ]]; then
  echo "请用 sudo 运行：sudo $0 /path/to/WakeSyncd.c"
  exit 1
fi

SRC="${1:-}"
if [[ -z "$SRC" || ! -f "$SRC" ]]; then
  echo "用法：sudo $0 /path/to/WakeSyncd.c"
  exit 1
fi

BIN="/usr/local/bin/WakeSyncd"
PLIST="/Library/LaunchDaemons/com.tommy.wakesync.plist"
LOG_OUT="/var/log/tommy.wakesync.out"
LOG_ERR="/var/log/tommy.wakesync.err"
LABEL="com.tommy.wakesync"

echo "==> 编译 $SRC → $BIN"
clang "$SRC" -o "$BIN" \
  -framework IOKit -framework CoreFoundation
  # -I/System/Library/Frameworks/IOKit.framework/Headers \
 #  -I/System/Library/Frameworks/CoreFoundation.framework/Headers

strip "$BIN"
chmod 755 "$BIN"
chown root:wheel "$BIN"

echo "==> 生成 LaunchDaemon plist → $PLIST"
cat > "$PLIST" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key> <string>com.lymos.timesyncd</string>

  <key>ProgramArguments</key>
  <array>
    <string>/usr/local/bin/WakeSyncd</string>
  </array>

  <!-- 开机自启 + 保持存活 -->
  <key>RunAtLoad</key> <true/>
  <key>KeepAlive</key> <true/>

  <key>StandardOutPath</key> <string>/var/log/tommy.wakesync.out</string>
  <key>StandardErrorPath</key> <string>/var/log/tommy.wakesync.err</string>
</dict>
</plist>
PLIST

chmod 644 "$PLIST"
chown root:wheel "$PLIST"

# 日志文件（可选）
touch "$LOG_OUT" "$LOG_ERR"
chmod 644 "$LOG_OUT" "$LOG_ERR"
chown root:wheel "$LOG_OUT" "$LOG_ERR"

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

echo "==> 完成！当前状态："
launchctl print system/"$LABEL" 2>/dev/null || echo "(不同 macOS 版本输出不同，未打印属正常)"

echo
echo "日志查看："
echo "  tail -f $LOG_OUT"
echo "  tail -f $LOG_ERR"
echo
echo "验证运行(几秒后)："
echo "  sudo log show --last 3m --predicate 'process == \"WakeSyncd\"' --info"
