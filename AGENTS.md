# AGENTS.md: NCC, Novation Circuit Companion (Felucca fork for the M-VAVE FM-1)

Instructions for any AI coding assistant (Claude, Copilot, Codex, Cursor, Gemini ..) and any human working on this repo.
Read this first, then **`docs/HANDOFF.md`** (where the work stands, what is next).

## Who and how
- Owner: **Remco** (GitHub: Fenderberg). He speaks and dictates **Dutch**: **always answer him in Dutch**. Dictation
  errors are common ("ferm" = FM, "M1/F1/MWave" = FM-1, "formeer" = firmware, "NC" = Novation Circuit Tracks).
- He is a musician, new to firmware development. Explain changes in plain words, keep steps small, **show a plan before
  larger changes**, and ask before choices that are his (sound, design, what goes public).
- Code, comments, commit messages, the manual and the site: **English**, in the style of the upstream code (dense,
  explanatory comments, C99, fixed point).
- **Never flash the FM-1 and never push to GitHub, publish a release or the site without Remco's explicit OK, each
  time.** An OK covers what it names, not what comes after.
- Report honestly: what was tested on the host, what on the device, what not at all.

## What NCC is
Firmware that makes the FM-1 a live companion to a **Novation Circuit Tracks (NC)**:
- The NC plays the notes (its two MIDI tracks) and sends the clock; its **8 macro knobs** shape the sound via CCs
  1 2 5 11 71 74 12 13 (OSCILLATOR, OSC MOD, AMP ENV, FILTER ENV, FILTER FREQ, RESONANCE, MODULATION, FX).
- The FM-1 has **two parts**: L on MIDI channel 3 (the NC's MIDI 1), R on channel 4 (MIDI 2); output **SPLIT** by
  default (L left, R right, for the NC's two audio inputs).
- The FM-1's panel is for **live jamming**: picking sounds (SELECT = category, ALGORITHM = engine, PRESETS = sound;
  none of them goes round; SELECT only acts on SOUND), pages on PLAY (tap next, hold 0.5 s previous),
  the arp (locked to the clock, HOLD on REC), Drone (SEQ), effects (FX opens on SENDS,
  throws on ENV / LFO). **No sequencer.** The NC is the boss.
- **Live tweaking without glitches is the most important requirement.** Less to learn, more to play.

Design docs (Dutch): `docs/NC-companion-ontwerp.md` (design), `docs/NC-companion-verkenning.md` (Felucca explored),
`docs/NC-mijlpaal-*-plan.md` (milestone plans). User docs (English): `README.md`, `docs/MANUAL.md`, `CHANGELOG.md`.

## Architecture rules
1. **Layers**: platform (upstream, untouched) → engines (+ a macro table) → performance layer (macros with slew, clock
   and arp, holds, sends and throws) → UI. The UI talks only to the performance layer and the adapters.
2. **Stay mergeable with upstream** (hugelton/Felucca): new things in **new files** behind the build flag
   `FELUCCA_LIVEUI=1`; edits to upstream files small and marked `/* NC: ... */`. Without the flag it builds Felucca.
3. The engine interface `firmware/engines/src/felucca_engine.h` (MIT, a submodule) stays as it is.
4. Stored formats don't change: NCC keeps its state in fields Felucca does not use (see below).

## Chip rules (JieLi AC791N, pi32v2): breaking these breaks or bricks things
- **Integers only**: no FPU, no float. Fixed point (Q15, 32-bit phases). **No 64-bit division** (64-bit multiply is
  fine). No C library (`printf`, `malloc`, `sin`).
- **Division by zero does not trap**: it gives garbage. Guard every divisor.
- **Audio ISR**: a block is 32 samples (CTL). Nothing slow in it: no screen, no flash, no waiting. Main loop ↔ ISR via
  `volatile`, one writer per variable.
- **Main loop** returns within a few ms (USB, updater, watchdog). **Screen**: draw only what changed.
- **Flash**: an erase silences audio ~50 ms: only save when quiet.
- **Memory is tight**: current dev image 578416 B of ~618 KB; RAM 61896 / 73728 B; pool 359464 / 368640 B. Check the
  size report of every build.
- No register access outside `firmware/hal/`, no calls into chip ROM (the build refuses both).

## NCC's code (all behind FELUCCA_LIVEUI)
| File | What |
|---|---|
| `firmware/src/perf_layer.c` | The macro layer: NC knobs → engine parameters per engine (`NCM_MAP`, popup words), slew. Audio ISR |
| `firmware/src/live_ui.c` | The panel's input: pages, parts, sound picking (filters, not round), arp, Drone, FX mode |
| `firmware/src/live_draw.c` | The screen (Circuit style): part boxes with category / engine pills, SOUND's status line, popups |
| `firmware/src/ncc_library.c` | One list of all sounds (NCC's own, presets, FM6's DX7 bank, user presets), each with a category from its name (`nl_cat`) |
| `firmware/src/ncc_extra_macros.c`, `ncc_extra_presets.c` | Macro tables and 46 patches for PHYS/GRAIN/VOICE/LOFI/WHEEL/NOISE/SAMPLE |
| `firmware/src/ncc_formant_math.c`, `ncc_noise.c` | NCC-only safe arithmetic adapters; engine interface/submodule unchanged |
| `firmware/src/ncc_sample_data.c` | Validated built-in/user XIP sample addressing |
| `firmware/src/ncc_presets.c` | NCC's own sounds (`NCC_SND`: engine, values) |
| `firmware/src/ncc_drone_fade.c` | Smooth REPLACE fade-out/in shared by all engines; event core writes, render cores read |
| `firmware/src/ncc_drone_overlap.c` | Per-voice REPLACE crossfade within the existing engine/shared voice budgets |
| `firmware/src/ncc_seq.c` | Arp on the clock's grid, Drone, the bar in clock pulses (`ncc_bar`). Included by `seq.c`, audio ISR |
| `firmware/src/ncc_input.c`, `ncc_settings.c` | Dual note ownership, local key snapshots, stored Dual/Drone modes; included by `seq.c` |
| `firmware/src/ncc_pages.c` | Settings on GLO and focused sound editing on EDIT, using existing descriptors and FM6 patch writer |
| `firmware/src/ncc_sound_defaults.c` | Zero delay/reverb sends after a sound load; included by `ui.c` |
| `firmware/src/juno_chorus.c` | Juno chorus, drift, SPLIT output. Audio ISR |
| `firmware/src/eng_acid.c` | ACID, NCC's 303 (engine 14) |
| `firmware/src/eng_loop.c` | LOOP, one-bar drum loops (engine 15). **Coming soon**: hidden by `NCC_LOOP 0` in `core.h` |
| `firmware/src/editor_ncc.c` | Editor protocol commands 96..100 for the NCC Manager (library, load on a part, parts, settings) |
| `web/ncc/manager.html`, `manager.js` | The NCC Manager (Web MIDI); reuses `web/editor.html`'s PROTO section |
| `web/emu/ncc_demo.c`, `skin-purple.css` | The emulator's demo Circuit, the purple FM-1 |
| `web/site/`, `tools/ncc_site.py` | The site (GitHub Pages): style, pictures, generator (manual from `docs/MANUAL.md`) |
| `tools/gen_loops.py` | LOOP's built-in loops → `build/gen/ncc_loops.h` |
| `tests/ncc_test.c`, `tests/ncc_*.c` | NCC behavior/audio tests; extra engine CPU comparisons |

Hooks marked `/* NC: */` in `felucca.c`, `main.c`, `fx.c`, `seq.c`, `midi_clock.c`, `midi_control.c`, `voice.c`,
`eng_fm6.c`, `fm6_core.c`, `ui_draw.c`, `editor.c`, `engines.c`, `core.h`, `tools/build.py`, `web/emu/*`.

Stored without a format change: a part's Juno character in `P_ED_FX` (chorus | drift << 2); SPLIT in `G_ENGGO`
(`NCC_SPLIT()`, `core.h`; 1 = STEREO); Dual/Drone modes in retired `G_SYNC` (marked bits, `ncc_settings.c`).
Engines 14 (ACID) and 15 (LOOP) need `NENGINES 16`; favourites refuse 14+; LOOP
is left out of Felucca's own engine list (`NENG_SHOWN`).

## Build, test, run: `tools/ncc_dev.sh`
Everything goes through one script (VS Code: Terminal → Run Task). It knows the Mac's detours (Colima, the build mirror
outside Dropbox, the right pythons):

```bash
sh tools/ncc_dev.sh test            # host tests + regression: run after every change
sh tools/ncc_dev.sh gen             # build/gen (needed once, and after changing a generator)
sh tools/ncc_dev.sh build           # firmware -> build/ncc.fwsc
sh tools/ncc_dev.sh emu demo        # browser emulator with a demo Circuit, localhost:8767
sh tools/ncc_dev.sh emu-test        # WebAssembly panel/MIDI integration and a screen capture
sh tools/ncc_dev.sh manager         # NCC Manager, localhost:8768 (?mock=1 without an FM-1)
sh tools/ncc_dev.sh release 0.4     # release build from HEAD -> build/rel/
sh tools/ncc_dev.sh site 0.4        # the site with that release -> build/site, localhost:8769
sh tools/ncc_dev.sh info | flash | publish-site   # device and publishing: only with Remco's OK
```

- `tests/ncc_test.c` must say `ncc: all ok`. Add a test for every new behaviour, in its style.
- The regression (`tests/regress.c`) checks golden renders: a sound change fails it. Update goldens only on purpose
  (`GOLDEN_UPDATE=1 build/host/regress_liveui tests/ncc_golden.txt tests/cpu_baseline.txt`) and say so in the commit.
  "2 CPU entries over budget" on the Mac is measuring noise.
- Upstream's full suite: `tests/run_tests.sh` (after a build; on the Mac two Felucca groups fail by design of the Mac).
- Setup on a new machine: `sh tools/setup_env.sh` (toolchain, SDK files, Pillow), Emscripten for the emulator, mido +
  python-rtmidi for the installer. See `docs/HANDOFF.md`.

## Flashing and releasing (only after Remco's OK)
- `sh tools/ncc_dev.sh flash` (or the web installer). Rescue: hold **OCT−** while switching on = update mode.
- Identity: dev builds `FM-1_700`, releases `FM-1_7XY` (`ncc-X.Y.fwsc`, `tools/build.py --release X.Y`).
- A release: test on the device first, then `gh release create vX.Y` with `build/rel/release-X.Y/ncc-X.Y.fwsc` and
  `build/rel/ncc-X.Y.zip`, then the site with that package. Keep `CHANGELOG.md` in step.

## Licence
GPL-3.0-only (Felucca); engines MIT; FM6 core Apache-2.0. New files get the SPDX header `GPL-3.0-only`. Sharing a
build means sharing the source. Never ship copyrighted recordings (e.g. the original Amen break) or DX7 ROM banks.
