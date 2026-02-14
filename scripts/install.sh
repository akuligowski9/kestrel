#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
APP_NAME="Kestrel"
APP_BUNDLE="/Applications/${APP_NAME}.app"

echo "Building Kestrel core..."
cmake -B "$PROJECT_DIR/build" -G Ninja -S "$PROJECT_DIR" -Wno-dev
cmake --build "$PROJECT_DIR/build"

echo "Building KestrelBar menu bar app..."
cd "$PROJECT_DIR/macos-app"
swift build -c release

echo "Packaging ${APP_NAME}.app..."
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"

# Copy binaries
cp "$PROJECT_DIR/macos-app/.build/release/KestrelBar" "$APP_BUNDLE/Contents/MacOS/KestrelBar"
cp "$PROJECT_DIR/build/core/kestrel" "$APP_BUNDLE/Contents/MacOS/kestrel"

# Copy default configs and icon
cp -r "$PROJECT_DIR/configs" "$APP_BUNDLE/Contents/Resources/configs"
cp "$PROJECT_DIR/macos-app/Resources/AppIcon.icns" "$APP_BUNDLE/Contents/Resources/AppIcon.icns"

# Create Info.plist
cat > "$APP_BUNDLE/Contents/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>KestrelBar</string>
    <key>CFBundleIdentifier</key>
    <string>com.kestrel.monitor</string>
    <key>CFBundleName</key>
    <string>Kestrel</string>
    <key>CFBundleDisplayName</key>
    <string>Kestrel</string>
    <key>CFBundleVersion</key>
    <string>0.1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>LSUIElement</key>
    <true/>
</dict>
</plist>
PLIST

echo ""
echo "Installed to $APP_BUNDLE"
echo "Launch from Applications or run: open '$APP_BUNDLE'"
