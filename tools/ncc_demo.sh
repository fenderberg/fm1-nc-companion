#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NC: NCC in the browser. Builds the browser emulator with NCC (FELUCCA_LIVEUI=1), the page named "Novation Circuit
# Companion" on a purple / pink FM-1, into build/emu-demo, serves it on localhost and opens it (Chrome or Edge: Web Audio; "Switch on" starts it).
# DEMO=1 adds the demo player (NCC_DEMO=1, web/emu/ncc_demo.c: a Circuit Tracks playing chords on L and an acid line
# on R, the clock, the macros moving).
#   tools/ncc_demo.sh            build, serve on port 8767, open
#   DEMO=1 tools/ncc_demo.sh     with the demo player
#   NO_SERVE=1 tools/ncc_demo.sh only build
#   PORT=9000 tools/ncc_demo.sh
# Needs Emscripten (brew install emscripten) and build/gen (one tools/build.py or the generators).
set -e
cd "$(dirname "$0")/.."
PORT="${PORT:-8767}"
[ "${DEMO:-0}" = 1 ] && DFLAG=-DNCC_DEMO=1 || DFLAG=
EMU_OUT=build/emu-demo EMU_TITLE="Novation Circuit Companion" EMU_SKIN=purple EMU_CFLAGS="-DFELUCCA_LIVEUI=1 $DFLAG" sh web/emu/build.sh
[ "${NO_SERVE:-0}" = 1 ] && exit 0
URL="http://localhost:$PORT/"
echo "ncc_demo: $URL  (Ctrl-C stops the server)"
if [ -d "/Applications/Google Chrome.app" ]; then
    (sleep 1; open -a "Google Chrome" "$URL") &
else
    (sleep 1; open "$URL") &
fi
exec /usr/bin/python3 -m http.server "$PORT" --bind 127.0.0.1 --directory build/emu-demo
