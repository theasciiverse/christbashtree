# Changes from upstream

This is the Asciiverse packaging of Sergio Lepore's ChristBASHTree
(github.com/sergiolepore/ChristBASHTree, the `upstream` remote), a
Christmas tree drawn with bash and `tput`. Its history is kept whole, up
to upstream's last commit `bfea545` (2018-01-13); everything after it is
ours, on branch `asciiverse`. Public domain under the Unlicense
(`LICENSE`), like the original.

## The C port (`christbashtree.c`)

The scripts, `tree-EN.sh` and `tree-ES.sh`, stay as they are; the
catalog ships `christbashtree.c`, a port to one C99 file with no library
but the C library, so it is one binary on Linux, macOS and Windows and
needs no bash, `tput` or terminfo. `christbashtree` draws the English
tree, `christbashtree es` the Spanish one.

Kept from the script, checked against its output in a 100x30 pty: the
tree's ten rows of green bold stars from the middle column (`cols / 2`),
the yellow `mWm` trunk, the red bold greeting with next year's number,
the lights (a row from 3 to 11, a column between the stars, as the
script computes them), the colours cycling 0 to 7, two rounds of 35
lights in which round 2 puts out round 1's lights and round 1 puts out
none (so round 2's lights pile up until a round 1 light lands on one and
is put out, as in the script), the word CODE (CODIGO) redrawn a letter at
a time in the newest light's colour, and the pace: the script spends
about 55 ms per light here (15 ms plus 10 ms per letter in the port).

Changed:

- Any key quits, Ctrl+C included, as do SIGTERM and SIGHUP; the terminal
  is put back every way (the script handled only Ctrl+C, with
  `tput reset`). It runs on the alternate screen with the cursor hidden.
- A resize redraws the whole picture, lights included, for the new size
  (the script drew once and kept writing at the old places).
- The picture (the script's rows 2 to 15) sits in the middle of the
  window, not two rows from the top; it stays centred across.
- Nothing is drawn outside the window, and line wrapping is off, so a
  small window shows part of the tree instead of a scrambled one.
- Windows: the console's VT mode draws it (Windows 10 1809 and later);
  Ctrl+C and keys through the console API.

## Packaging

- `asciiverse/build.sh` builds `out/christbashtree` with the compiler
  and flags from the environment; `asciiverse/package.sh` packs
  `dist/christbashtree-<target>.tar.gz` (.zip for Windows) with
  `LICENSE`; `.github/workflows/release.yml` builds static musl Linux
  (x86_64, aarch64), macOS (x86_64, arm64) and Windows x86_64 (MinGW).
- `asciiverse/record-preview.sh` records the catalog preview,
  `asciiverse/christbashtree.cast`, 30 seconds as av starts it.
- Checked here: ASan and UBSan clean through resizes from 100x30 down to
  2x2 and up to 130x40; q and Ctrl+C restore the terminal. The Windows
  build, cross-built with MinGW 14, was run under Wine 10 in tmux: the
  tree and its lights draw, q quits with status 0, but Wine's console
  printed some escape sequences literally, so the drawing on real Windows
  is unchecked. macOS not built here.
