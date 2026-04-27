#!/usr/bin/env bash
set -e

PREFIX=/usr/local
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib"
INC_DIR="$PREFIX/include"

BINARY=cortex-vm
LIBRARY=libcortex-vm.a
HEADER=cortex-vm.h

# privilege helper
if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
    echo "==> Removal requires elevated privileges."
fi

# remove
removed=0

remove() {
    local path="$1"
    if [ -e "$path" ]; then
        $SUDO rm -f "$path"
        echo "==> Removed $path"
        removed=$((removed + 1))
    else
        echo "==> Not found (skipping): $path"
    fi
}

remove "$BIN_DIR/$BINARY"
remove "$LIB_DIR/$LIBRARY"
remove "$INC_DIR/$HEADER"

# post-remove (Linux only)
if [ "$(uname -s)" = "Linux" ]; then
    $SUDO ldconfig
fi

echo ""
if [ "$removed" -eq 0 ]; then
    echo "Nothing to remove — cortex-vm does not appear to be installed."
else
    echo "cortex-vm uninstalled ($removed file(s) removed)."
fi
