#!/bin/bash
set -e
LOGFILE="/tmp/fnkeys-uninstall.log"
exec > >(tee -a "$LOGFILE") 2>&1

APP_NAME="FnKeys.app"
DST_APP="/Applications/$APP_NAME"
PLIST="/Library/LaunchAgents/com.tommy.fnkeys.plist"


# ----------------------------
# 卸载 LaunchAgent
# ----------------------------
if [ -f "$PLIST" ]; then
    echo "[INFO] Unloading LaunchAgent $PLIST"
    # 停止当前 GUI session
    USER_UID=$(stat -f "%u" /dev/console 2>/dev/null || echo "")
    if [ -n "$USER_UID" ]; then
        /bin/launchctl bootout gui/$USER_UID "$PLIST" 2>/dev/null || true
    fi

    # 删除 plist
    /bin/rm -f "$PLIST"
    echo "[INFO] LaunchAgent removed"
fi

# 查杀可能残留的 FnKeys 进程
ps aux | grep '[F]nKeys' | awk '{print $2}' | xargs -r kill -9
# USER_UID=$(stat -f "%u" /dev/console)
# USER_NAME=$(id -rn $USER_UID)
# sudo -u "$USER_NAME" osascript -e 'tell application "FnKeys" to quit' || true
# sudo -u "$USER_NAME" pkill -f "/Applications/FnKeys.app/Contents/MacOS/FnKeys" || true

# ----------------------------
# 删除 App
# ----------------------------
if [ -d "$DST_APP" ]; then
    echo "[INFO] Removing $DST_APP"
    /bin/rm -rf "$DST_APP"
fi

# ----------------------------
# 可选：删除临时日志
# ----------------------------
/bin/rm -f /tmp/fnkeys.log /tmp/fnkeys.err

echo "[INFO] FnKeys uninstalled successfully"
exit 0
