#!/bin/bash
# Creates a styled DMG with background, icon positions, and hidden file positions.
# Usage: ./create-styled-dmg.sh <staging-dir> <background.png> <output.dmg> <volname>

set -e

STAGING="$1"
BACKGROUND="$2"
OUTPUT="$3"
VOLNAME="${4:-q3now}"
TEMP_DMG="${OUTPUT}.rw.dmg"

# Calculate size (staging + 20MB headroom)
SIZE_KB=$(du -sk "$STAGING" | cut -f1)
SIZE_MB=$(( (SIZE_KB / 1024) + 20 ))

# Create writable DMG
rm -f "$TEMP_DMG" "$OUTPUT"
hdiutil create -size "${SIZE_MB}m" -fs HFS+ -volname "$VOLNAME" "$TEMP_DMG"

# Mount
MOUNT_DIR="/Volumes/$VOLNAME"
hdiutil attach -readwrite -noverify -noautoopen "$TEMP_DMG"
sleep 1
echo "Mounted at: $MOUNT_DIR"

# Copy contents
cp -R "$STAGING"/* "$MOUNT_DIR/"

# Add Applications symlink + set custom icon so it's visible on DMG
ln -s /Applications "$MOUNT_DIR/Applications"
# Copy the system Applications folder icon onto the symlink
APPS_ICON="/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/ApplicationsFolderIcon.icns"
if [ -f "$APPS_ICON" ] && which fileicon > /dev/null 2>&1; then
    fileicon set "$MOUNT_DIR/Applications" "$APPS_ICON" 2>/dev/null || true
fi

# Add background
mkdir -p "$MOUNT_DIR/.background"
cp "$BACKGROUND" "$MOUNT_DIR/.background/bg.png"

# Set Finder window properties via AppleScript
osascript <<APPLESCRIPT
tell application "Finder"
    tell disk "$VOLNAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set bounds of container window to {200, 120, 870, 590}
        set viewOptions to icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 128
        set text size of viewOptions to 14
        set background picture of viewOptions to file ".background:bg.png"
        set position of item "Wired.app" of container window to {90, 200}
        set position of item "Applications" of container window to {390, 200}
        set position of item ".background" of container window to {10, 900}
        set position of item ".DS_Store" of container window to {50, 900}
        set position of item ".fseventsd" of container window to {90, 900}
        close
        open
        update without registering applications
        delay 2
        close
    end tell
end tell
APPLESCRIPT

# Ensure .DS_Store is written
sync

# Unmount
hdiutil detach "$MOUNT_DIR"

# Compress
hdiutil convert "$TEMP_DMG" -format UDZO -o "$OUTPUT"
rm -f "$TEMP_DMG"

echo "Created: $OUTPUT ($(du -h "$OUTPUT" | cut -f1))"
