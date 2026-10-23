# CLAUDE.md: NCC, Novation Circuit Companion (Felucca fork for the M-VAVE FM-1)

## Who and how
- Owner: Remco (GitHub: Fenderberg). He speaks and dictates **Dutch**: always answer in Dutch. Dictation errors are common ("ferm" = FM, "M1/F1/MWave" = FM-1, "formeer" = firmware, "NC" = Novation Circuit Tracks).
- He is a musician, new to firmware development. Explain changes in plain words, keep steps small, and show a plan before writing larger changes.
- Code, comments and commit messages: **English**, in the style of the upstream code (dense, explanatory comments, C99, fixed point).

## What we are building
Firmware that makes the FM-1 a live companion to a **Novation Circuit Tracks (NC)**:
- The NC plays the notes (its two MIDI tracks) and sends the MIDI clock; its **8 macro knobs** shape the sound via CCs.
- The FM-1 has **two parts**: L = FM6 (DX7-style, 6-op), R = ANALOG (to become Juno-style), on MIDI channels **3 and 4** by default (the NC's MIDI 1 / MIDI 2 tracks).
- The 8 macros follow the NC's **panel legends** (Oscillator, Osc Mod, Amp Env, Filter Env, Filter Freq, Resonance, Modulation, FX) and its factory MIDI template (CC 1 2 5 11 71 74 12 13).
- The FM-1's panel is for **live jamming**: choosing sounds, the arp (always locked to the clock, with Arp-hold), Drone-hold, and effects (incl. throws). **No sequencer.**
- **Live tweaking without glitches is the most important requirement.** Engines must stay swappable.

Read before starting work:
- `docs/NC-companion-ontwerp.md`: the design (in Dutch): controls, pages, macros, arp, holds, FX, architecture.
- `docs/NC-companion-verkenning.md`: the exploration of Felucca (in Dutch): what exists, what is missing, constraints.

## Architecture rules
1. **Layers**: platform (upstream, untouched) → engines (+ a small adapter / macro table) → performance layer (macros with slew, clock and arp, holds, FX sends and throws) → UI. The UI talks only to the performance layer and the adapters.
2. **Stay mergeable with upstream** (hugelton/Felucca): put new things in **new files** (e.g. `firmware/src/live_ui.c`, `firmware/src/perf_layer.c`) behind a build flag (`FELUCCA_LIVEUI=1`). Keep edits to upstream files small and marked with `/* NC: ... */`.
3. Don't remove the old Felucca UI or sequencer at first; compile them out with the flag only when needed (RAM).
4. The engine interface is `firmware/engines/src/felucca_engine.h` (MIT). Don't change it unless really needed; the submodule is upstream's.

## Chip rules (the FM-1: JieLi AC791N, pi32v2): violating these breaks or bricks things
- **Integers only**: no FPU, no float. Fixed point (Q15, 32-bit phases). **No 64-bit division** (64-bit multiply is OK). No C library (`printf`, `malloc`, `sin`).
- **Division by zero does not trap** on the device; it gives a wrong value. Guard every divisor. (Example found in this exploration: `fx.c` `beat_samples()` divides by `song.g[G_BPM]`, and UBSan caught a zero there in the drumfuzz SONG run.)
- **Audio ISR**: a block is 32 samples (CTL), a half buffer 128 frames (2.9 ms). Nothing slow in it: no screen, no flash, no waiting. Main loop ↔ ISR via `volatile`, one writer per variable.
- **Main loop** must return within a few ms (USB, updater, watchdog).
- **Screen**: draw only what changed (a full screen is ~80 ms).
- **Flash**: a sector erase silences audio ~50 ms: only save when it is quiet, never while playing, never in a loop.
- **Memory is tight**: RAM .data+.bss 60,984 of 73,728 B; pool 328,896 of 368,640 B (v1.6.1 build). The delay line alone is 128 KB. Check the size report of every build.
- No register access outside `firmware/hal/`, no calls into chip ROM: the build refuses both.

## Build and test
- Set up once: `sh tools/setup_env.sh` (toolchain, the 3 SDK files, Pillow, first build). On Linux x86-64 no Docker is needed; on macOS Docker Desktop must run.
- Build: `./build.sh` → `build/felucca.fwsc` (+ size report: watch RAM and pool). NCC itself: `FELUCCA_LIVEUI=1 ./build.sh` (without the flag the firmware is Felucca as is).
- Without a firmware build (macOS without Docker): `build/gen` can be made with Python alone (`DYLD_LIBRARY_PATH=/opt/homebrew/lib` for Pillow's Raqm, and `pip3 install fonttools`), then single host tests compile with `cc -w -Ibuild/gen -Ifirmware/src`, e.g. `tests/ncc_test.c`. `tests/run_tests.sh` itself stops without `build/felucca.fwsc`.
- Tests: `sh tests/run_tests.sh` (after a build). On Linux the drumfuzz test needs `-include stdint.h` (an upstream portability issue): add it to its compile line in `tests/run_tests.sh` in our fork.
- The regression suite (`tests/regress.c`) checks golden renders, CPU per preset (`tests/cpu_baseline.txt`, +25 %) and target loop cost (`tests/target_budget.txt`, +10 %). A sound change fails it: update goldens only on purpose (`GOLDEN_UPDATE=1`, `BUDGET_UPDATE=1`) and say so in the commit.
- Add host tests for new behaviour (macro slew, arp grid lock, holds) in the style of `tests/*_test.c`. NCC's: `tests/ncc_test.c` (built with the flag on). Upstream's `midi_control_test` tests Felucca's own CC meanings and fails with the flag on, by design.
- NCC's code: `firmware/src/perf_layer.c` (macros, audio ISR), `firmware/src/juno_chorus.c` (Juno chorus + SPLIT, audio ISR), `firmware/src/ncc_seq.c` (arp grid + Drone-hold, included by `seq.c`, audio ISR), `firmware/src/eng_acid.c` (ACID, NCC's 303, engine 14 behind the flag), `firmware/src/eng_loop.c` (LOOP, coming soon: left out of NCC's lists by `NCC_LOOP 0` in `core.h`, the engine itself built; one-bar drum loops on the clock's 16ths, the note a gate, IDM knobs; engine 15; its loops from `tools/gen_loops.py` → `build/gen/ncc_loops.h`, 11025 Hz 8 bits, 22 KB a loop: watch the image size, ~618 KB at most), `firmware/src/live_ui.c` (input), `firmware/src/ncc_library.c` (the sounds of the five engines in one list by category: SELECT the category, ALGORITHM the engine, PRESETS through what fits; the category from the name), `firmware/src/live_draw.c` (screen, Circuit style) and `firmware/src/ncc_presets.c` (main loop), hooks marked `/* NC: */` in `felucca.c`, `main.c`, `fx.c`, `seq.c`, `midi_control.c`, `voice.c` (drift), `eng_fm6.c` (CEG), `ui_draw.c` (splash), `tools/build.py`, `tools/size_fns.py`, `web/emu/felucca_web.c`. Engines 14 (ACID) and 15 (LOOP) need NENGINES 16 with the flag (LOOP is left out of Felucca's own engine list, `NENG_SHOWN`: the EDIT layer's white keys): favourites refuse engines 14+ (rows 14/15 hold MIDI LEARN and the prefs). The NCC build's regression uses `tests/ncc_golden.txt` (Felucca's goldens + ACID's): keep it in step when upstream changes a golden. Stored without a format change: a part's Juno character in `P_ED_FX` (chorus | drift << 2), SPLIT in `G_ENGGO` (both unused by Felucca; SPLIT is the default, `G_ENGGO = 1` means STEREO: `NCC_SPLIT()` in `core.h`, the NCC regression builds with `NCC_SPLIT_DEFAULT=0`). FX is a toggle (the FX mode: `kb_lock`, SELECT turns PERFORM SENDS DELAY REVERB CHORUS INSERT).
- Building on Remco's Mac (Colima + Rosetta): run `tools/build.py` from a mirror outside Dropbox (`rsync -a --delete --exclude build/ --exclude .git/ <repo>/ ~/.cache/ncc-build/`), because parallel containers on the Dropbox mount fail with "Resource deadlock avoided"; then copy `build/felucca.fwsc` back.
- Local emulator: `sh tools/ncc_demo.sh` builds the emulator with NCC (page title "Novation Circuit Companion", the purple / pink FM-1: `web/emu/skin-purple.css`, `EMU_SKIN`) into `build/emu-demo`, serves it on localhost:8767 and opens Chrome. `DEMO=1` adds a built-in Circuit player (`web/emu/ncc_demo.c`, `NCC_DEMO=1`: clock, Start, chords on channel 3, an acid line on 4, macro sweeps).
- NCC Manager: `sh tools/ncc_manager.sh` builds `web/ncc/manager.html` + `manager.js` into `build/ncc-manager` (Felucca editor's protocol code copied in as `proto.js`: `web/editor.html` PROTO-BEGIN..END) and serves it on localhost:8768 (`?mock=1`: a simulated FM-1 with NCC). NCC's editor commands 96..100 (library, load on a part, parts, settings): `firmware/src/editor_ncc.c`, hooked in `editor.c`.
- Site (GitHub Pages, the `gh-pages` branch, fenderberg.github.io/fm1-nc-companion): `tools/ncc_site.py build/ncc-X.Y.fwsc X.Y OUT` builds it (Felucca's `web/make_site.py` installer and package in NCC's words, the NCC Manager, the emulator with its demo Circuit, a front page); publish OUT as the `gh-pages` branch (a force push of a fresh one-commit repo).
- Browser emulator: `web/emu/build.sh` needs Emscripten (`EMU_OUT` picks the output folder) (`brew install emscripten` on a Mac). It accepts Web MIDI (notes; CC 20–27 as panel knobs), so the NC can be tested against it.

## Flashing (only Remco does this, on his Mac, after he agrees)
- Never flash without asking. Before the first own build: test the rescue path (hold **OCT−** while switching on = updater) and keep the official V15 and a released Felucca at hand.
- Install: Chrome/Edge web installer, or `python3 tools/fm1_install.py build/felucca.fwsc` (`--info` shows what runs now).
- Package identity (`tools/build.py` `PRODUCT`): **`FM-1_700`**, releases `FM-1_7XY` (`ncc-X.Y.fwsc`). The installers match only the model ("FM-1" before the `_`), the Firmware Hub the SHA-256.
- After a flash: run the test list in the design doc and update the FM-1 log (Remco keeps it in his claude.ai project).

## Licence
GPL-3.0-only (Felucca); engines MIT; FM6 core Apache-2.0. New files get the SPDX header `GPL-3.0-only`. Sharing a build means sharing the source.
