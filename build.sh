#!/usr/bin/env bash
# build.sh — compile RAMEN TIME
#
#   ./build.sh              build -> build/ramen-time.gbc
#   ./build.sh --assets     redraw + re-convert the art first
#   ./build.sh --playtest   build, then run the scripted playtest
#   ./build.sh --clean      remove build output
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
OUT="build/ramen-time.gbc"
TITLE="RAMENTIME"

# Prefer the project venv (pillow / numpy / pyboy live there).
if [ -x "$HERE/.venv/bin/python3" ]; then
  export PATH="$HERE/.venv/bin:$PATH"
  PYTHON="$HERE/.venv/bin/python3"
else
  PYTHON="python3"
fi

do_assets=0
do_play=0
for arg in "$@"; do
  case "$arg" in
    --assets)   do_assets=1 ;;
    --playtest) do_play=1 ;;
    --clean)    rm -rf build; echo "cleaned"; exit 0 ;;
    -h|--help)  sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $arg (try --help)" >&2; exit 2 ;;
  esac
done

find_gbdk() {
  if [ -n "${GBDKDIR:-}" ] && [ -x "${GBDKDIR%/}/bin/lcc" ]; then
    echo "$(cd "${GBDKDIR%/}" && pwd -P)/"; return
  fi
  for d in /opt/gbdk "$HOME/gbdk" "$HOME/opt/gbdk" /usr/local/gbdk; do
    if [ -x "$d/bin/lcc" ]; then echo "$(cd "$d" && pwd -P)/"; return; fi
  done
  if command -v lcc >/dev/null 2>&1; then
    d="$(dirname "$(dirname "$(command -v lcc)")")"; echo "$(cd "$d" && pwd -P)/"; return
  fi
  echo ""
}

GBDKDIR="$(find_gbdk)"
if [ -z "$GBDKDIR" ]; then
  echo "error: GBDK-2020 not found. Install 4.5.0 and export GBDKDIR=/path/to/gbdk/" >&2
  exit 1
fi
export GBDKDIR
LCC="$GBDKDIR/bin/lcc"
echo "GBDKDIR = $GBDKDIR"
echo "PYTHON  = $PYTHON ($($PYTHON -V 2>&1))"

if [ "$do_assets" = "1" ]; then
  echo "== regenerating art =="
  "$PYTHON" assets/make_art.py
fi

mkdir -p build
echo "== compiling =="
# -Wm-yc     CGB compatible (header 0x80); -Wm-yC would be CGB-only
# -Wm-yt0x1B MBC5 + RAM + battery
# -Wm-yn     ROM title, <= 11 chars
"$LCC" -Wm-yc -Wm-yt0x1B -Wm-yn"$TITLE" -o "$OUT" src/main.c src/gen/*.c

echo "built $OUT ($(wc -c < "$OUT") bytes)"
echo "  playtest: $PYTHON tools/playtest.py --rom $OUT --script 'wait:8,snap:title,press:start:4,wait:20,snap:play'"

if [ "$do_play" = "1" ]; then
  echo "== playtest =="
  SCRIPT_FILE="$HERE/logs/playtest-script.txt"
  if [ -f "$SCRIPT_FILE" ]; then
    SCRIPT="$(tr -d '\n' < "$SCRIPT_FILE")"
  else
    SCRIPT="wait:8,snap:title,press:start:4,wait:20,snap:play"
  fi
  "$PYTHON" tools/playtest.py --rom "$OUT" --script "$SCRIPT" \
    --out build/playtest-strip.png --frames-dir build/frames --strict
fi
