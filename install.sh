#!/usr/bin/env bash
set -e

PREFIX=/usr/local
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib"
INC_DIR="$PREFIX/include"

BINARY=cortex-vm
LIBRARY=libcortex-vm.a
HEADER=cortex-vm.h

# sanity check
cd "$(dirname "$0")"

if [ ! -f Makefile ]; then
    echo "error: cannot find Makefile — run this script from the cortex-vm source directory" >&2
    exit 1
fi

# submodules
echo "==> Initialising submodules..."
git submodule update --init --recursive

# build
echo "==> Building $BINARY..."
make

echo "==> Building lib$BINARY..."
make lib

# test
if command -v pytest >/dev/null 2>&1; then
    echo "==> Running tests..."
    if ! pytest; then
        echo ""
        echo "error: tests failed — aborting installation." >&2
        exit 1
    fi
else
    echo "==> pytest not found — skipping tests (install pytest to enable pre-install checks)."
fi

# privilege helper
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
    echo "==> Installation requires elevated privileges."
fi

# install
echo "==> Installing binary  -> $BIN_DIR/$BINARY"
$SUDO install -m 755 "$BINARY" "$BIN_DIR/$BINARY"

echo "==> Installing library -> $LIB_DIR/$LIBRARY"
$SUDO install -m 644 "lib/$LIBRARY" "$LIB_DIR/$LIBRARY"

echo "==> Installing header  -> $INC_DIR/$HEADER"
$SUDO install -m 644 "lib/$HEADER" "$INC_DIR/$HEADER"

# post-install (Linux only)
if [ "$(uname -s)" = "Linux" ]; then
    $SUDO ldconfig
fi

echo ""
echo "cortex-vm installed successfully."
echo "  binary : $BIN_DIR/$BINARY"
echo "  library: $LIB_DIR/$LIBRARY"
echo "  header : $INC_DIR/$HEADER"
