#!/usr/bin/env sh

# This script downloads the CMake binary and installs it in $PREFIX directory
# (the cmake executable will be in $PREFIX/bin). By default $PREFIX is
# ~/.local but can we changes with --prefix <PREFIX> argument.

# This is mostly suitable for CIs, not end users.

set -e

VERSION=3.20.0

if [ "$1" = "--prefix" ]; then
    PREFIX="$2"
else
    PREFIX=~/.local
fi

OS=$(uname -s)
SHA256=3f8aeb2907d96cd4dc03955228b2be3f8b58cff65704bd9ce4599589253c8de8


BIN=$PREFIX/bin

if test -f $BIN/cmake && ($BIN/cmake --version | grep -q "$VERSION"); then
    echo "CMake $VERSION already installed in $BIN"
else
    FILE=cmake-$VERSION-linux-x86_64.tar.gz
    URL=https://cmake.org/files/v3.20/$FILE
    ERROR=0
    TMPFILE=$(mktemp --tmpdir cmake-$VERSION-$OS-x86_64.XXXXXXXX.tar.gz)
    echo "Downloading CMake ($URL)..."
    curl -s "$URL" > "$TMPFILE"

    if type -p sha256sum > /dev/null; then
        SHASUM="sha256sum"
    else
        SHASUM="shasum -a256"
    fi

    if ! ($SHASUM "$TMPFILE" | grep -q "$SHA256"); then
        echo "Checksum mismatch ($TMPFILE)"
        exit 1
    fi
    mkdir -p "$PREFIX"
    tar xzf "$TMPFILE" -C "$PREFIX" --strip 1
    rm $TMPFILE
fi