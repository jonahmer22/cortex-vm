#!/usr/bin/env bash
set -e

PREFIX=/usr/local
BIN_DIR="$PREFIX/bin"
LIB_DIR="$PREFIX/lib"
INC_DIR="$PREFIX/include"

BINARY=cortex
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
"$VENV_DIR/bin/pip" install -r requirements.txt # I like seeing what it's doing

# build with PGO
echo "==> Building instrumented binary for PGO profiling..."
PGO_DIR="$(pwd)/build/pgo"
mkdir -p "$PGO_DIR"
make clean
make PGO_CFLAGS="-fprofile-generate=$PGO_DIR" \
     PGO_LDFLAGS="-fprofile-generate=$PGO_DIR"

echo "==> Collecting PGO profile data..."
# one run per logical CPU, minimum 5
if command -v nproc >/dev/null 2>&1; then
    _NCPU=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    _NCPU=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 5)
else
    _NCPU=5
fi
_NRUNS=$(( _NCPU > 5 ? _NCPU : 5 ))
echo "    Launching $_NRUNS parallel benchmark runs..."
_PIDS=()
for _i in $(seq 1 "$_NRUNS"); do
    "$VENV_DIR/bin/python" benchmarks/run.py --no-graphs >/dev/null 2>&1 &
    _PIDS+=($!)
done
for _pid in "${_PIDS[@]}"; do
    wait "$_pid" || true   # || true: partial profile data is still useful
done

# pgo-use calls `make clean` which would delete build/pgo/*.gcda, so
# stash the profile data outside build/ and restore it after the clean.
echo "==> Rebuilding $BINARY with PGO..."
_PGO_TMP=$(mktemp -d)
cp "$PGO_DIR"/*.gcda "$_PGO_TMP/" 2>/dev/null || true
make clean
mkdir -p "$PGO_DIR"
cp "$_PGO_TMP"/*.gcda "$PGO_DIR/" 2>/dev/null || true
rm -rf "$_PGO_TMP"
make PGO_CFLAGS="-fprofile-use=$PGO_DIR -fprofile-correction" \
     PGO_LDFLAGS="-fprofile-use=$PGO_DIR -fprofile-correction"

echo "==> Building lib$BINARY with PGO..."
make lib \
     PGO_CFLAGS="-fprofile-use=$PGO_DIR -fprofile-correction" \
     PGO_LDFLAGS="-fprofile-use=$PGO_DIR -fprofile-correction"

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
echo "cortex installed successfully."
echo "  binary : $BIN_DIR/$BINARY"
echo "  library: $LIB_DIR/$LIBRARY"
echo "  header : $INC_DIR/$HEADER"
