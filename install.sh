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
    echo "error: cannot find Makefile - run this script from the cortex-vm source directory" >&2
    exit 1
fi

# submodules
echo "==> Initialising submodules..."
git submodule update --init --recursive

# python environment
if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 not found - required for the test suite." >&2
    exit 1
fi

VENV_DIR=".venv"
echo "==> Creating Python virtual environment ($VENV_DIR)..."
python3 -m venv "$VENV_DIR"

echo "==> Installing Python dependencies..."
"$VENV_DIR/bin/pip" install --quiet -r requirements.txt

# build
echo "==> Building $BINARY..."
make

echo "==> Building lib$BINARY..."
make lib

# test
echo "==> Running tests..."
if ! "$VENV_DIR/bin/pytest"; then
    echo ""
    echo "error: tests failed - aborting installation." >&2
    exit 1
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
