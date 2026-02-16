#!/bin/bash
# Build script for mango plugins
# This compiles plugins independently so they can be updated without recompiling mango
# Usage: build.sh <plugin_source_file>

set -e

if [ $# -lt 1 ]; then
    echo "Error: No plugin source file specified"
    echo "Usage: $0 <plugin_source_file>"
    exit 1
fi

PLUGIN_SOURCE="$1"
if [ ! -f "$PLUGIN_SOURCE" ]; then
    echo "Error: Plugin source file not found: $PLUGIN_SOURCE"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-.}"
INSTALL_DIR="${INSTALL_DIR:-.}"
PLUGIN_INSTALL_DIR="${INSTALL_DIR}/lib/mango/plugins"

# Extract plugin name from source file (without extension)
PLUGIN_NAME=$(basename "$PLUGIN_SOURCE" .c)

# Determine which compiler flags to use
if command -v pkg-config &> /dev/null; then
    WLROOTS_CFLAGS=$(pkg-config --cflags wlroots-0.19 2>/dev/null || pkg-config --cflags wlroots 2>/dev/null || echo "-I/usr/include")
    WLROOTS_LIBS=$(pkg-config --libs wlroots-0.19 2>/dev/null || pkg-config --libs wlroots 2>/dev/null || echo "-lwlroots")
    WAYLAND_CFLAGS=$(pkg-config --cflags wayland-server)
    WAYLAND_LIBS=$(pkg-config --libs wayland-server)
else
    echo "Error: pkg-config not found. Please install pkg-config."
    exit 1
fi

# Ensure build directory exists
mkdir -p "$BUILD_DIR"
mkdir -p "$PLUGIN_INSTALL_DIR"

# Common compiler flags
COMMON_CFLAGS="-fPIC -shared -g -Wno-unused-function -std=c99"
LINK_FLAGS="-Wl,--allow-shlib-undefined -Wl,-z,nodelete"

# Compile plugin
echo "Building $PLUGIN_NAME plugin..."
gcc \
    $COMMON_CFLAGS \
    $WLROOTS_CFLAGS \
    $WAYLAND_CFLAGS \
    -I"$PROJECT_ROOT/src" \
    -DWLR_USE_UNSTABLE \
    -DXWAYLAND \
    -D_POSIX_C_SOURCE=200809L \
    -o "$BUILD_DIR/lib${PLUGIN_NAME}.so" \
    "$PLUGIN_SOURCE" \
    $WLROOTS_LIBS \
    $WAYLAND_LIBS \
    $LINK_FLAGS

echo "Successfully built $PLUGIN_NAME plugin: $BUILD_DIR/lib${PLUGIN_NAME}.so"

# Copy to install directory if different from build directory
if [ "$BUILD_DIR" != "$INSTALL_DIR" ]; then
    cp "$BUILD_DIR/lib${PLUGIN_NAME}.so" "$PLUGIN_INSTALL_DIR/lib${PLUGIN_NAME}.so"
    echo "Installed to: $PLUGIN_INSTALL_DIR/lib${PLUGIN_NAME}.so"
fi

echo "Plugin build completed!"
