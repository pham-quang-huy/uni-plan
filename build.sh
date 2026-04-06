#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/Build/CMake"

echo "Configuring..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug

echo "Building..."
cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.logicalcpu)"

# Install: symlink binary to ~/bin/
mkdir -p ~/bin
BINARY="$BUILD_DIR/uni-plan"
ln -sf "$BINARY" ~/bin/uni-plan

echo ""
echo "Installed: ~/bin/uni-plan -> $BINARY"
echo "Run: uni-plan --version"
