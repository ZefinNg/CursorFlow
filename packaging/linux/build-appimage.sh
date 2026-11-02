#!/usr/bin/env bash
# Linux AppImage 打包（05 号文档 §5，任务 5.4）
# 依赖：linuxdeployqt（https://github.com/probonopd/linuxdeployqt）、X11 运行库随系统
# 用法：cmake --build build --target install 后执行本脚本
set -euo pipefail

APP_DIR=${1:-build/appdir}
BUILD_DIR=${2:-build}

cmake --install "$BUILD_DIR" --prefix "$APP_DIR/usr"

# .desktop 与图标（XDG 规范，AppImage 必需）
mkdir -p "$APP_DIR/usr/share/applications" "$APP_DIR/usr/share/icons/hicolor/scalable/apps"
cat > "$APP_DIR/usr/share/applications/cursorflow.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=CursorFlow
GenericName=Input Activity Tracker
Comment=轻量级鼠标键盘活动统计工具
Exec=CursorFlow
Icon=cursorflow
Categories=Utility;
EOF
cp packaging/linux/cursorflow.svg "$APP_DIR/usr/share/icons/hicolor/scalable/apps/cursorflow.svg" 2>/dev/null || true

# 注意：CursorFlow 需 X11 会话（Wayland 启动时显示降级提示，04 号文档 §6）
linuxdeployqt "$APP_DIR/usr/bin/CursorFlow" -appimage -verbose=1
