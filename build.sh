#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/Build/CMake"
PRESET="dev"
RUN_TESTS=0
INSTALL=1
CLEAN=0

Usage()
{
    cat <<'EOF'
Usage: ./build.sh [--tests] [--clean] [--no-install]

Builds uni-plan with the shared CMake preset:
  output:  Build/CMake/uni-plan
  install: ~/bin/uni-plan

Options:
  --tests       Configure with UPLAN_TESTS=ON and run uni-plan-tests
  --clean       Build with --clean-first
  --no-install  Skip cmake --install
EOF
}

DetectJobs()
{
    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.logicalcpu 2>/dev/null && return
    fi

    if command -v nproc >/dev/null 2>&1; then
        nproc && return
    fi

    getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tests)
            PRESET="dev-tests"
            RUN_TESTS=1
            ;;
        --clean)
            CLEAN=1
            ;;
        --no-install)
            INSTALL=0
            ;;
        -h|--help)
            Usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            Usage >&2
            exit 2
            ;;
    esac
    shift
done

echo "Configuring..."
(
    cd "$SCRIPT_DIR"
    cmake --preset "$PRESET"
)

echo "Building..."
BUILD_ARGS=(--build "$BUILD_DIR" --parallel "$(DetectJobs)")
if [[ "$CLEAN" -eq 1 ]]; then
    BUILD_ARGS+=(--clean-first)
fi
cmake "${BUILD_ARGS[@]}"

BINARY="$BUILD_DIR/uni-plan"

echo ""
echo "Built: $BINARY"

if [[ "$INSTALL" -eq 1 ]]; then
    mkdir -p "$HOME/bin"
    if [[ -L "$HOME/bin/uni-plan" ]]; then
        rm "$HOME/bin/uni-plan"
    fi
    cmake --install "$BUILD_DIR" --prefix "$HOME" --component runtime
    echo "Installed: $HOME/bin/uni-plan"
fi

if [[ "$RUN_TESTS" -eq 1 ]]; then
    echo ""
    echo "Running tests..."
    "$BUILD_DIR/uni-plan-tests"
fi

echo "Run: uni-plan --version"
