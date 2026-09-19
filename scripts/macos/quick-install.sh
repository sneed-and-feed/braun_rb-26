#!/bin/bash
# ==============================================================================
# BRAUN RB-26 — One-Line Zero-Click Installer for macOS
# Run in macOS Terminal:
#   curl -fsSL https://raw.githubusercontent.com/sneed-and-feed/braun_rb-26/main/scripts/macos/quick-install.sh | bash
# ==============================================================================

set -e

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
ORANGE='\033[0;33m'
RED='\033[0;31m'
RESET='\033[0m'

echo ""
echo -e "${BOLD}${ORANGE}================================================================${RESET}"
echo -e "${BOLD}${ORANGE}       BRAUN RB-26 STUDIO REVERB — macOS ONE-LINE INSTALLER      ${RESET}"
echo -e "${BOLD}${ORANGE}================================================================${RESET}"
echo ""

if [[ "$(uname)" != "Darwin" ]]; then
    echo -e "${RED}Error: This script is only intended for macOS.${RESET}"
    exit 1
fi

ARCH="$(uname -m)"
echo -e "${CYAN}System Architecture:${RESET} ${BOLD}${ARCH}${RESET}"

REPO="sneed-and-feed/braun_rb-26"
TMP_DIR="/tmp/braun_rb26_installer_$$"

mkdir -p "$TMP_DIR"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "Finding latest macOS Universal release from GitHub ($REPO)..."
LATEST_JSON=$(curl -sSL "https://api.github.com/repos/${REPO}/releases/latest" 2>/dev/null || true)

DOWNLOAD_URL=$(echo "$LATEST_JSON" | grep -o 'https://[^"]*macOS-Universal\.zip' | head -n 1 || true)

if [ -z "$DOWNLOAD_URL" ]; then
    # Fallback to finding latest release asset by tag search
    DOWNLOAD_URL=$(curl -sSL "https://api.github.com/repos/${REPO}/releases" | grep -o 'https://[^"]*macOS-Universal\.zip' | head -n 1 || true)
fi

if [ -z "$DOWNLOAD_URL" ]; then
    echo -e "${RED}Could not automatically find macOS-Universal.zip on GitHub Releases.${RESET}"
    echo "Please visit https://github.com/${REPO}/releases to download manually."
    exit 1
fi

ZIP_FILE="$TMP_DIR/rb26_mac.zip"
echo -e "Downloading: ${CYAN}${DOWNLOAD_URL}${RESET}..."
curl -sSL -L -o "$ZIP_FILE" "$DOWNLOAD_URL"

echo "Extracting archive..."
unzip -q -o "$ZIP_FILE" -d "$TMP_DIR/extracted"

# Plugin destinations
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
APP_DIR="$HOME/Applications"

mkdir -p "$AU_DIR" "$VST3_DIR" "$CLAP_DIR" "$APP_DIR"

# Install AU
AU_SRC=$(find "$TMP_DIR/extracted" -type d -name "BRAUN_RB26.component" | head -n 1 || true)
if [ -n "$AU_SRC" ]; then
    echo -e "Installing ${BOLD}Audio Unit (AU)${RESET} -> $AU_DIR/..."
    rm -rf "$AU_DIR/BRAUN_RB26.component"
    cp -R "$AU_SRC" "$AU_DIR/"
    xattr -cr "$AU_DIR/BRAUN_RB26.component" 2>/dev/null || true
    echo -e "${GREEN}  ✓ AU installed & Gatekeeper unquarantined${RESET}"
fi

# Install VST3
VST3_SRC=$(find "$TMP_DIR/extracted" -type d -name "BRAUN_RB26.vst3" | head -n 1 || true)
if [ -n "$VST3_SRC" ]; then
    echo -e "Installing ${BOLD}VST3${RESET} -> $VST3_DIR/..."
    rm -rf "$VST3_DIR/BRAUN_RB26.vst3"
    cp -R "$VST3_SRC" "$VST3_DIR/"
    xattr -cr "$VST3_DIR/BRAUN_RB26.vst3" 2>/dev/null || true
    echo -e "${GREEN}  ✓ VST3 installed & Gatekeeper unquarantined${RESET}"
fi

# Install CLAP
CLAP_SRC=$(find "$TMP_DIR/extracted" -type f -name "BRAUN_RB26.clap" | head -n 1 || true)
if [ -n "$CLAP_SRC" ]; then
    echo -e "Installing ${BOLD}CLAP${RESET} -> $CLAP_DIR/..."
    rm -f "$CLAP_DIR/BRAUN_RB26.clap"
    cp "$CLAP_SRC" "$CLAP_DIR/"
    xattr -cr "$CLAP_DIR/BRAUN_RB26.clap" 2>/dev/null || true
    echo -e "${GREEN}  ✓ CLAP installed & Gatekeeper unquarantined${RESET}"
fi

# Install Standalone
APP_SRC=$(find "$TMP_DIR/extracted" -type d -name "BRAUN_RB26.app" | head -n 1 || true)
if [ -n "$APP_SRC" ]; then
    echo -e "Installing ${BOLD}Standalone${RESET} -> $APP_DIR/..."
    rm -rf "$APP_DIR/BRAUN_RB26.app"
    cp -R "$APP_SRC" "$APP_DIR/"
    xattr -cr "$APP_DIR/BRAUN_RB26.app" 2>/dev/null || true
    echo -e "${GREEN}  ✓ Standalone app installed & Gatekeeper unquarantined${RESET}"
fi

# Refresh macOS Audio Units cache
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo -e "${BOLD}${GREEN}================================================================${RESET}"
echo -e "${BOLD}${GREEN}     BRAUN RB-26 SUCCESSFULLY INSTALLED ON YOUR MAC!           ${RESET}"
echo -e "${BOLD}${GREEN}================================================================${RESET}"
echo ""
echo "Open your DAW (Logic Pro, Ableton Live, Reaper, FL Studio) or launch:"
echo "  open ~/Applications/BRAUN_RB26.app"
echo ""
