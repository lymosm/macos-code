#!/bin/bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
OUT_DIR="$PROJECT_DIR/dist-pkg"
PKGROOT="$OUT_DIR/pkgroot"
TMP="$OUT_DIR/tmp"

rm -rf "$PKGROOT" "$TMP"
mkdir -p "$PKGROOT/Applications"
mkdir -p "$PKGROOT/Library/LaunchAgents"
mkdir -p "$TMP"

# 源文件和输出路径
SRC_C="$PROJECT_DIR/Script/keys.c"
SRC_M="$PROJECT_DIR/Script/osd.m"
APP_DIR="$PKGROOT/Applications/FnKeys.app"
BIN_DIR="$APP_DIR/Contents/MacOS"
INFO_PLIST="$APP_DIR/Contents/Info.plist"

mkdir -p "$BIN_DIR" "$APP_DIR/Contents/Resources"

# 编译二进制
echo "[INFO] Compiling -> $BIN_DIR/fnkeys"
clang "$SRC_C" "$SRC_M" -o "$BIN_DIR/fnkeys" \
  -framework IOKit -framework CoreFoundation -framework ApplicationServices -framework Cocoa
chmod 755 "$BIN_DIR/fnkeys"

# 生成 Info.plist
cat > "$INFO_PLIST" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>FnKeys</string>
    <key>CFBundleIdentifier</key>
    <string>tommy.asus.fnkeys</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSUIElement</key>
    <true/>
</dict>
</plist>
EOF

# 签名（ad-hoc）
codesign -s - --deep --force "$APP_DIR"

# 安装 LaunchAgent（改用 APP 内部路径）
cat > "$PKGROOT/Library/LaunchAgents/tommy.asus.fnkeys.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
   "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
   <key>Label</key>
   <string>tommy.asus.fnkeys</string>
   <key>ProgramArguments</key>
   <array>
      <string>/Applications/FnKeys.app/Contents/MacOS/fnkeys</string>
   </array>
   <key>RunAtLoad</key>
   <true/>
   <key>KeepAlive</key>
   <true/>
</dict>
</plist>
EOF
chmod 644 "$PKGROOT/Library/LaunchAgents/tommy.asus.fnkeys.plist"

# 生成 pkg
TMP_PKG="$TMP/fnkeys.pkg"
pkgbuild --root "$PKGROOT" \
  --identifier "tommy.asus.fnkeys" \
  --version "1.0" \
  --install-location / \
  "$TMP_PKG"

# 直接生成最终安装包
mkdir -p "$OUT_DIR"
mv "$TMP_PKG" "$OUT_DIR/installFnKey.pkg"

echo "[INFO] Created $OUT_DIR/installFnKey.pkg"

# 生成卸载 pkg —— 使用 nopayload + uninstall_scripts 中的 postinstall
pkgbuild --nopayload \
  --identifier "tommy.asus.fnkeys" \
  --version "1.0" \
  --scripts "$PROJECT_DIR/Pkg/uninstall.sh" \
  "$OUT_DIR/uninstallFnKey.pkg"

echo "[INFO] Created $OUT_DIR/uninstallFnKey.pkg"

# 清理临时
rm -rf "$TMP"

echo "[OK] Packages in $OUT_DIR"
exit 0



