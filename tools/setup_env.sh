#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NC-Companion (Felucca fork): set up a build environment and build once.
# Tested on Linux x86-64 (Ubuntu 24.04, Claude Code cloud) on 2026-10-20: Felucca v1.6.1 builds without Docker.
# On macOS, build.sh runs the toolchain in a linux/amd64 Docker container: install and start Docker Desktop first.
#   tools/setup_env.sh            # toolchain + SDK files + Pillow, then ./build.sh
#   SKIP_BUILD=1 tools/setup_env.sh
set -e
cd "$(dirname "$0")/.."

# 0. the engines submodule
git submodule update --init --recursive

# 1. Python: Pillow for the build, mido + python-rtmidi for tools/fm1_install.py (optional)
python3 -c 'import PIL' 2>/dev/null || pip3 install --break-system-packages Pillow 2>/dev/null || pip3 install Pillow
python3 -c 'import mido' 2>/dev/null || pip3 install --break-system-packages mido python-rtmidi 2>/dev/null || true

# 2. JieLi toolchain (clang 4.0.1 for pi32v2) -> ~/.jieli/toolchain
[ -x "$HOME/.jieli/toolchain/pi32v2/bin/clang" ] || sh tools/get_toolchain.sh

# 3. The three files of the JieLi AC79 SDK the package needs (Apache-2.0), tag AC79NN_SDK_V1.2.1_2023-12-13.
#    The full SDK is large; a blobless, sparse clone fetches only these. Gitee can reset connections: retries.
SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
TAG=AC79NN_SDK_V1.2.1_2023-12-13
if [ ! -f "$SDK/cpu/wl82/tools/uboot.boot" ] || [ ! -s "$SDK/cpu/wl82/tools/cfg/eq_cfg_hw.bin" ]; then
    [ -d "$SDK/.git" ] || git clone --depth 1 --branch "$TAG" --filter=blob:none --no-checkout \
        https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git "$SDK"
    mkdir -p "$SDK/cpu/wl82/tools/cfg"
    for f in cpu/wl82/tools/uboot.boot cpu/wl82/tools/cfg_tool.bin cpu/wl82/tools/cfg/eq_cfg_hw.bin; do
        i=0
        until (cd "$SDK" && git show "HEAD:$f" > "$f.tmp" 2>/dev/null && [ -s "$f.tmp" ]); do
            i=$((i + 1)); [ $i -ge 6 ] && { echo "setup_env: could not fetch $f from Gitee"; exit 1; }
            echo "setup_env: retry $f ($i)"; sleep 3
        done
        mv "$SDK/$f.tmp" "$SDK/$f"
    done
fi
# tools/build.py checks the SHA-256 of these three files itself
export AC79_SDK="$SDK"

# 4. build once (build/felucca.fwsc) unless SKIP_BUILD=1
[ "${SKIP_BUILD:-0}" = 1 ] || ./build.sh
echo "setup_env: ok. Tests: sh tests/run_tests.sh (Linux: see CLAUDE.md, drumfuzz needs -include stdint.h)"
