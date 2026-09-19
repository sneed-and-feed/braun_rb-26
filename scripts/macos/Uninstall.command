#!/bin/bash
# ==============================================================================
# BRAUN RB-26 — Uninstaller for macOS
# Double-click this script in Finder to remove installed AU, VST3, CLAP, and Standalone.
# ==============================================================================

echo "=========================================================="
echo "          BRAUN RB-26 — macOS Uninstaller                 "
echo "=========================================================="
echo ""

read -p "Are you sure you want to remove BRAUN RB-26 from your system? (y/N): " confirm
if [[ "$confirm" != [yY] && "$confirm" != [yY][eE][sS] ]]; then
    echo "Uninstallation canceled."
    exit 0
fi

echo "Removing components..."
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/BRAUN_RB26.component"
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/BRAUN_RB26.vst3"
rm -f  "$HOME/Library/Audio/Plug-Ins/CLAP/BRAUN_RB26.clap"
rm -rf "$HOME/Applications/BRAUN_RB26.app"

echo "Flushing AudioComponentRegistrar cache..."
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Uninstallation complete. All BRAUN RB-26 files have been removed."
read -n 1 -s -r -p "Press any key to close..."
echo ""
