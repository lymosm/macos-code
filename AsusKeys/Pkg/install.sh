#!/bin/bash
set -e

APP_NAME="FnKeys.app"
DST_APP="/Applications/$APP_NAME"
SRC_APP="$PKG_ROOT/Applications/$APP_NAME"  # pkg 内临时路径

# ----------------------------
# 安装 FnKeys.app 到 /Applications
# ----------------------------
if [ -d "$SRC_APP" ]; then
    echo "[INFO] Installing $APP_NAME to $DST_APP"
    /bin/cp -R "$SRC_APP" "$DST_APP"
    /bin/chmod -R 755 "$DST_APP"
    /usr/sbin/chown -R root:wheel "$DST_APP"
else
    echo "[WARN] $SRC_APP not found, skipping app installation"
fi

# ----------------------------
# 创建 LaunchAgent 用于开机启动 App
# ----------------------------
PLIST="/Library/LaunchAgents/com.tommy.fnkeys.plist"

cat <<EOF > "$PLIST"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.tommy.fnkeys</string>
    <key>ProgramArguments</key>
    <array>
        <string>$DST_APP/Contents/MacOS/FnKeys</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardErrorPath</key>
    <string>/tmp/fnkeys.err</string>
    <key>StandardOutPath</key>
    <string>/tmp/fnkeys.log</string>
</dict>
</plist>
EOF

# 设置权限
/bin/chmod 644 "$PLIST"
/usr/sbin/chown root:wheel "$PLIST"

# ----------------------------
# 立即加载 LaunchAgent（当前 GUI session）
# ----------------------------
USER_UID=$(stat -f "%u" /dev/console 2>/dev/null || echo "")
if [ -n "$USER_UID" ]; then
    echo "[INFO] Bootstrapping LaunchAgent for GUI session UID $USER_UID"
    /bin/launchctl bootstrap gui/$USER_UID "$PLIST" 2>/dev/null || true
    /bin/launchctl kickstart -k "gui/$USER_UID/com.tommy.fnkeys" 2>/dev/null || true
fi

exit 0

