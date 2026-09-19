#!/bin/bash
# ==============================================================================
# BRAUN RB-26 — One-Click Standalone Launcher for macOS
# Double-click this script in Finder to run BRAUN RB-26 immediately with zero setup.
# ==============================================================================

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

echo "=========================================================="
echo "          Launching BRAUN RB-26 Standalone...             "
echo "=========================================================="

APP_PATH="$DIR/Standalone/BRAUN_RB26.app"

if [ ! -d "$APP_PATH" ]; then
    APP_PATH="$HOME/Applications/BRAUN_RB26.app"
fi

if [ ! -d "$APP_PATH" ]; then
    echo "Error: BRAUN_RB26.app not found. Please run Install.command first."
    read -n 1 -s -r -p "Press any key to exit..."
    exit 1
fi

echo "Removing macOS Gatekeeper quarantine attribute..."
xattr -cr "$APP_PATH" 2>/dev/null || true

echo "Opening BRAUN RB-26..."
open "$APP_PATH"
