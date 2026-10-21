#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NC: NCC's local demo. Builds the browser emulator with NCC (FELUCCA_LIVEUI=1) and its demo player (NCC_DEMO=1,
# web/emu/ncc_demo.c: a Circuit Tracks playing chords on L and an acid line on R, the clock, the macros moving) into
# build/emu-demo, serves it on localhost and opens it (Chrome or Edge: Web Audio; "Switch on" starts it).
#   tools/ncc_demo.sh            build, serve on port 8767, open
#   NO_SERVE=1 tools/ncc_demo.sh only build
#   PORT=9000 tools/ncc_demo.sh
# Needs Emscripten (brew install emscripten) and build/gen (one tools/build.py or the generators).
set -e
cd "$(dirname "$0")/.."
PORT="${PORT:-8767}"
EMU_OUT=build/emu-demo EMU_CFLAGS="-DFELUCCA_LIVEUI=1 -DNCC_DEMO=1" sh web/emu/build.sh
[ "${NO_SERVE:-0}" = 1 ] && exit 0
URL="http://localhost:$PORT/"
echo "ncc_demo: $URL  (Ctrl-C stops the server)"
if [ -d "/Applications/Google Chrome.app" ]; then
    (sleep 1; open -a "Google Chrome" "$URL") &
else
    (sleep 1; open "$URL") &
fi
exec /usr/bin/python3 -m http.server "$PORT" --bind 127.0.0.1 --directory build/emu-demo
