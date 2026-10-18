#!/bin/sh
# Builds christbashtree into out/ with the compiler and flags from the
# environment, so one script serves every target (.github/workflows/
# release.yml): one C99 file, the C library alone.
#
#   asciiverse/build.sh                                         # here
#   LDFLAGS="-static -s" asciiverse/build.sh                    # Alpine
#   CC=x86_64-w64-mingw32-gcc LDFLAGS="-static -s" EXE=.exe asciiverse/build.sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
CFLAGS="${CFLAGS:--O2}"
LDFLAGS="${LDFLAGS:-}"
EXE="${EXE:-}"
mkdir -p out
$CC -std=c99 $CFLAGS -Wall -Wextra christbashtree.c $LDFLAGS -o "out/christbashtree$EXE"
echo "built out/christbashtree$EXE"
