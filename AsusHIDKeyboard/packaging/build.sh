#!/bin/bash
set -euo pipefail

# PROJECT_DIR 由 Xcode 环境传入，或回退到 packaging 上一级
PROJECT_DIR="${PROJECT_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
OUT_DIR="$PROJECT_DIR/dist-pkg"
PKGROOT="$OUT_DIR/pkgroot"
TMP="$OUT_DIR/tmp"

# 清理旧产物（只清理本脚本产物，不碰 Xcode Build 目录）
rm -rf "$PKGROOT" "$TMP"
mkdir -p "$PKGROOT/usr/local/bin"
mkdir -p "$PKGROOT/Library/LaunchAgents"
mkdir -p "$TMP"

# 源文件和输出二进制路径
SRC_C="$PROJECT_DIR/Script/fnkeys.c"
DST_BIN="$PKGROOT/usr/local/bin/fnkeys"

echo "[INFO] Compiling $SRC_C -> $DST_BIN"

# 编译，包含你要求的 frameworks
clang "$SRC_C" -o "$DST_BIN" \
  -framework IOKit -framework CoreFoundation -framework ApplicationServices

chmod 755 "$DST_BIN"

# 拷贝 plist 到 pkgroot 的 LaunchAgents（注意：只是拷贝到 pkgroot 下，安装时会放到 /Library/LaunchAgents）
if [ -f "$PROJECT_DIR/packaging/launchd.plist" ]; then
  cp "$PROJECT_DIR/packaging/launchd.plist" "$PKGROOT/Library/LaunchAgents/com.asus.fnkeys.plist"
else
  echo "[WARN] packaging/launchd.plist not found"
fi
chmod 644 "$PKGROOT/Library/LaunchAgents/com.asus.fnkeys.plist" || true

# 生成临时 install pkg（供 productbuild 使用）
TMP_PKG="$TMP/fnkeys.pkg"
pkgbuild --root "$PKGROOT" \
  --identifier "com.asus.fnkeys" \
  --version "1.0" \
  --install-location / \
  --scripts "$PROJECT_DIR/packaging" \
  "$TMP_PKG"

# 用 productbuild 生成最终安装包（包含 GUI distribution）
if [ -f "$PROJECT_DIR/packaging/Distribution" ]; then
  productbuild --distribution "$PROJECT_DIR/packaging/Distribution" \
    --package-path "$TMP" \
    "$OUT_DIR/installFnKey.pkg"
else
  # fallback: 直接用 pkgbuild 的产物当 install 包
  mv "$TMP_PKG" "$OUT_DIR/installFnKey.pkg"
fi

echo "[INFO] Created $OUT_DIR/installFnKey.pkg"

# 生成卸载 pkg —— 使用 nopayload + uninstall_scripts 中的 postinstall
pkgbuild --nopayload \
  --identifier "com.asus.fnkeys" \
  --version "1.0" \
  --scripts "$PROJECT_DIR/packaging/uninstall_scripts" \
  "$OUT_DIR/uninstallFnKey.pkg"

echo "[INFO] Created $OUT_DIR/uninstallFnKey.pkg"

# 清理临时
rm -rf "$TMP"

echo "[OK] Packages in $OUT_DIR"
exit 0
