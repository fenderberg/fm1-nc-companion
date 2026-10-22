#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NC: the NCC Manager (web/ncc/manager.html): NCC's sounds, your sounds, DX7 banks, drum loops, backups and settings
# over USB-MIDI. Builds it into build/ncc-manager (the protocol code is Felucca editor's own: web/editor.html between
# PROTO-BEGIN and PROTO-END, as proto.js), serves it on localhost (Web MIDI with SysEx needs localhost or https) and
# opens Chrome (or Edge).
#   tools/ncc_manager.sh             build, serve on port 8768, open
#   NO_SERVE=1 tools/ncc_manager.sh  only build
#   PORT=9000 tools/ncc_manager.sh
# Without an FM-1: open http://localhost:8768/?mock=1 (a simulated FM-1 with NCC).
set -e
cd "$(dirname "$0")/.."
PORT="${PORT:-8768}"
O=build/ncc-manager
mkdir -p "$O"
{ echo '"use strict";'; sed -n '/\/\*PROTO-BEGIN\*\//,/\/\*PROTO-END\*\//p' web/editor.html; } > "$O/proto.js"
cp web/ncc/manager.html "$O/index.html"
cp web/ncc/manager.js web/fm1backup.js "$O/"
echo "ncc_manager: $O"
[ "${NO_SERVE:-0}" = 1 ] && exit 0
URL="http://localhost:$PORT/"
echo "ncc_manager: $URL  (Ctrl-C stops the server)"
if [ -d "/Applications/Google Chrome.app" ]; then
    (sleep 1; open -a "Google Chrome" "$URL") &
else
    (sleep 1; open "$URL") &
fi
exec /usr/bin/python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$O"
