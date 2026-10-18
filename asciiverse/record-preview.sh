#!/bin/sh
# Records the catalog preview at 100x30 the way av starts christbashtree
# (no arguments), ended by SIGTERM, which it answers by giving the
# terminal back. Needs asciinema 3 and out/christbashtree.
#
#   asciiverse/record-preview.sh [seconds]
set -eu
cd "$(dirname "$0")/.."
seconds=${1:-30}
repo=$PWD
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT
mkdir "$home/bin" && cp out/christbashtree "$home/bin/"
cd "$home"  # nothing in the recording names a local path
HOME="$home" PATH="$home/bin:$PATH" TERM=xterm-256color COLORTERM=truecolor LANG=C.UTF-8 \
asciinema rec --headless --overwrite -q \
  --window-size 100x30 --idle-time-limit 2 \
  --capture-env "TERM,COLORTERM,LANG" --title "Christmas Tree" \
  --command "timeout --foreground -s TERM $seconds christbashtree" \
  "$repo/asciiverse/christbashtree.cast"
ls -la "$repo/asciiverse/christbashtree.cast"
