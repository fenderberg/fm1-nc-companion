#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NC: every NCC chore in one place, as it runs on Remco's Mac (Apple silicon, Colima instead of Docker Desktop, the repo
# in Dropbox). Any editor or assistant can call it; VS Code's tasks (.vscode/tasks.json) do.
#
#   sh tools/ncc_dev.sh gen              build/gen from the generators (fonts, tables, samples, loops): host tests need it
#   sh tools/ncc_dev.sh test             NCC's host tests (UBSan), the regression (with and without NCC), the editor test
#   sh tools/ncc_dev.sh build            the firmware from the working tree -> build/ncc.fwsc (identity FM-1_700)
#   sh tools/ncc_dev.sh release X.Y      a release build from HEAD -> build/rel/release-X.Y/ncc-X.Y.fwsc + ncc-X.Y.zip
#   sh tools/ncc_dev.sh info             what the connected FM-1 runs
#   sh tools/ncc_dev.sh flash [FILE]     install FILE (default build/ncc.fwsc) on the FM-1: ONLY with Remco's OK
#   sh tools/ncc_dev.sh emu [demo]       the browser emulator (demo: with the built-in Circuit) on localhost:8767
#   sh tools/ncc_dev.sh manager          the NCC Manager on localhost:8768 (?mock=1: a simulated FM-1)
#   sh tools/ncc_dev.sh site X.Y         the site with release X.Y's package -> build/site, served on localhost:8769
#   sh tools/ncc_dev.sh publish-site     build/site as the gh-pages branch (GitHub Pages): ONLY with Remco's OK
#
# Why the detours: the JieLi toolchain runs in x86-64 containers (Colima with Rosetta); parallel containers on the
# Dropbox folder fail ("Resource deadlock avoided"), so builds run in a mirror (~/.cache/ncc-build); macOS strips
# DYLD_LIBRARY_PATH through /bin/sh, so tools/build.py is called directly with the python that has Pillow and
# fontTools (the framework 3.13; Homebrew's python3 is 3.15 without them, but has mido for the installer); the
# servers run from a copy outside Dropbox with the system python (the framework one hangs serving there).
set -e
cd "$(dirname "$0")/.."
REPO=$PWD
PY=${NCC_PY:-/Library/Frameworks/Python.framework/Versions/3.13/bin/python3}       # Pillow, fontTools
PYMIDI=${NCC_PYMIDI:-/opt/homebrew/bin/python3}                                     # mido, python-rtmidi
MIRROR=${NCC_MIRROR:-$HOME/.cache/ncc-build}
SERVE=${NCC_SERVE:-$HOME/.cache/ncc-serve}
export DYLD_LIBRARY_PATH=/opt/homebrew/lib JIELI_TOOLCHAIN=${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain} \
       AC79_SDK=${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}

serve() {   # serve DIR PORT: a copy outside Dropbox, the system python
    rm -rf "$SERVE/$2" && mkdir -p "$SERVE" && cp -R "$1" "$SERVE/$2"
    lsof -ti "tcp:$2" | xargs kill 2>/dev/null || true
    echo "serving http://localhost:$2/  (Ctrl-C stops it)"
    exec /usr/bin/python3 -m http.server "$2" --bind 127.0.0.1 --directory "$SERVE/$2"
}
colima_up() { colima status >/dev/null 2>&1 || colima start --vm-type vz --vz-rosetta; }
mirror() {  # the working tree, or with "head" the last commit (a release), into the mirror
    rsync -a --delete --exclude build/ --exclude .git/ --exclude keep/ "$REPO/" "$MIRROR/"
    [ "$1" = head ] && git -C "$REPO" archive HEAD | tar -x -C "$MIRROR"
    return 0
}

case "${1:-}" in
gen)
    mkdir -p build/gen
    for g in "gen_aa_font.py build/gen/ui_fonts.h --preset inter-tight" "gen_aa_icons.py build/gen/ui_icons.h" \
             "gen_aa_keycaps.py build/gen/ui_keycaps.h" "gen_ui_palettes.py build/gen/ui_palettes.h" \
             "gen_tables.py build/gen/felucca_tables.h" "gen_fm6_patches.py build/gen/felucca_fm6.h" \
             "gen_samples.py build/gen/felucca_samples.h" "gen_loops.py build/gen/ncc_loops.h"; do
        # shellcheck disable=SC2086
        "$PY" tools/$g
    done ;;
test)
    mkdir -p build/host
    cc -O1 -w -fsanitize=undefined -Ibuild/gen -Ifirmware/src -o build/host/ncc_test tests/ncc_test.c -lm
    build/host/ncc_test | grep -E "FAIL|^ncc: (all|FAILED)"
    cc -O1 -w -DNCC_LOOP=1 -Ibuild/gen -Ifirmware/src -o build/host/ncc_test_loop tests/ncc_test.c -lm
    build/host/ncc_test_loop | grep -E "FAIL|^ncc: (all|FAILED)" | sed 's/^/(NCC_LOOP=1) /'
    cc -O2 -w -DFELUCCA_LIVEUI=1 -DNCC_SPLIT_DEFAULT=0 -Ibuild/gen -Ifirmware/src -o build/host/regress_liveui tests/regress.c -lm
    build/host/regress_liveui tests/ncc_golden.txt tests/cpu_baseline.txt | grep "golden renders" | sed 's/^/(NCC) /'
    cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/regress tests/regress.c -lm
    build/host/regress tests/golden.txt tests/cpu_baseline.txt | grep "golden renders" | sed 's/^/(Felucca) /'
    cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/editor_test tests/editor_test.c -lm
    build/host/editor_test | tail -1
    echo "(2 CPU entries over budget on the Mac are its measuring noise, not a fault)" ;;
build)
    colima_up && mirror
    (cd "$MIRROR" && FELUCCA_LIVEUI=1 "$PY" tools/build.py | grep -E "image|package|FAIL")
    cp "$MIRROR/build/felucca.fwsc" build/ncc.fwsc && echo "-> build/ncc.fwsc" ;;
release)
    [ -n "${2:-}" ] || { echo "release X.Y"; exit 2; }
    colima_up && mirror head
    (cd "$MIRROR" && FELUCCA_LIVEUI=1 "$PY" tools/build.py --release "$2" | grep -E "image|package|FAIL")
    rm -rf build/rel && mkdir -p build/rel && cp -R "$MIRROR/build/release-$2" build/rel/
    (cd build/rel && zip -qr "ncc-$2.zip" "release-$2" && cat "release-$2/SHA256SUMS") ;;
info)
    "$PYMIDI" tools/fm1_install.py --info ;;
flash)
    f=${2:-build/ncc.fwsc}
    printf "Install %s on the FM-1? Only with Remco's OK. [y/N] " "$f"; read -r a; [ "$a" = y ] || exit 1
    echo y | "$PYMIDI" tools/fm1_install.py "$f" | tr '\033' '\n' | grep -v "^\[K\(writing\|checking\)" ;;
emu)
    if [ "${2:-}" = demo ]; then DEMO=1 NO_SERVE=1 sh tools/ncc_demo.sh; else NO_SERVE=1 sh tools/ncc_demo.sh; fi
    serve build/emu-demo 8767 ;;
manager)
    NO_SERVE=1 sh tools/ncc_manager.sh && serve build/ncc-manager 8768 ;;
site)
    [ -n "${2:-}" ] && [ -f "build/rel/release-$2/ncc-$2.fwsc" ] || { echo "site X.Y (after: release X.Y)"; exit 2; }
    "$PY" tools/ncc_site.py "build/rel/release-$2/ncc-$2.fwsc" "$2" build/site && serve build/site 8769 ;;
publish-site)
    [ -f build/site/index.html ] || { echo "no build/site (run: site X.Y)"; exit 2; }
    printf "Publish build/site as GitHub Pages? Only with Remco's OK. [y/N] "; read -r a; [ "$a" = y ] || exit 1
    P=$(mktemp -d) && cp -R build/site/. "$P/" && cd "$P" && git init -q -b gh-pages && git add -A &&
        git commit -q -m "NCC site" && git remote add origin "$(git -C "$REPO" remote get-url origin)" &&
        git -c http.postBuffer=524288000 push -f origin gh-pages ;;
*)
    sed -n '4,15p' "$0"; exit 2 ;;
esac
