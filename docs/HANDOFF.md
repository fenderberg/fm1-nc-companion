# Handoff: where NCC stands

Read **AGENTS.md** first (rules, code map, commands). This file is the state of the work, kept up to date by whoever
works on it last. Last update: 2026-10-23.

## Released and live
- **Release 0.3** (identity `FM-1_703`) is on GitHub and on the site; Remco's FM-1 runs it.
  - 0.3: clearer sound picking (not round, pills, status line, preview), the bar in clock pulses, DX7 bank names kept,
    LOOP hidden ("coming soon").
  - 0.2: sounds by category, 30 new sounds, LOOP, the NCC Manager. 0.1: the first release.
- **Site**: https://fenderberg.github.io/fm1-nc-companion/ (the `gh-pages` branch): home, manual (built from
  `docs/MANUAL.md`), web installer (0.3), NCC Manager, emulator with a demo Circuit. Dark, Circuit Tracks-like style;
  the FM-1 in the pictures is Remco's purple one. The demo video spot is a placeholder: set `VIDEO` in
  `tools/ncc_site.py` to a YouTube embed URL when there is one.
- Shared on Reddit (r/novationcircuit); people asked for the web installer and editor (done).

## On Remco's FM-1 (not in git)
- A DX7 bank "BANK0054" (32 voices) and user sample slot USR1 "LOOPS" (3 loops, unused while LOOP is hidden).
- A best-of DX7 bank he can load himself: `build/dx7/NCC-DX7-best-of.syx` (32 classics from the public DX7 archive;
  not committed: Yamaha's ROM voices are not ours to ship).

## In progress: Felucca's extra engines in NCC (requested 2026-10-23)
Remco: "doe de extra engines. hou de interface hoe je deze kiest as is. probeer de engines te vullen met zoveel mogelijk
goede patches." Nothing is written yet; the research is done:

| Engine (index) | Parameters (E0..E7) | Felucca's presets | Notes |
|---|---|---|---|
| PHYS (9) | MODEL (MODAL STRNG MEMB SYMP), STRC, BRIT, DAMP, POS, ACC, BOW, EXC (labels change per model: `phys_desc`) | BELL TREE, MARIMBA, PLUCK, BOWED METAL, KALIMBA, HAND DRUM, TOMS, DRONE STRING, HARP | poly 3; plucks, bells, mallets, bowed |
| GRAIN (8) | SRC (sample sets, USR1..3), POS, SIZE, DENS, PTCH, SPRD, RAND, TONE | CLOUD PAD, GLITCH, FROZEN, SHIMMER | pads and textures over SAMPLE's material |
| VOICE (5) | VOWL, VOWL2, TALK, SHIFT, BUZZ, BRTH, Q, RAND | CHOIR AAH, VOX LEAD, WOW BASS, WHISPER | formant voices, poly 4 |
| SAMPLE (4) | SET (PIANO FLUTE SAX, USR1..3), TUNE, BITS, LOOP, CUT, -, DRV, - | PIANO, FLUTE, SAX | `sampled`; only 3 real presets (one is an alias) |
| LOFI (3) | CHIP, WAVE, DUTY, CRSH, SWP, VIB, ARP, TONE | PULSE LD, WAVE BASS, ARP 8BIT, WAVE LEAD, STEP LEAD | chip voices |
| WHEEL (7) | REG (16 drawbar settings), SUB, BODY, TOP, PERC, CLICK, DRV, ROTR | FULL ORGAN, JAZZ PERC, GOSPEL, SOFT FLUTE, ROCK DRIVE | tonewheel organ |
| NOISE (11) | MODE (ANLG DUST LFSR META), COLR, FREQ, RES, TRK, DENS, DRFT, CRSH | WIND, RAIN, ARCADE, METAL | textures, risers |

Left out on purpose: DRUM (10: the Circuit has drums), SLICE (13: needs the user's samples), DIGITAL (1: retired).

The plan:
1. Add the engines to `LV_ENGS` / `LV_ENG_NAME` in `live_ui.c` (after ACID; names of at most ~6 letters fit the part
   box). The library, ALGORITHM and the manager pick them up from there. The interface stays as it is.
2. A `NCM_MAP` entry per engine in `perf_layer.c`: the NC's 8 knobs on what their labels mean for that engine, plus
   the popup words (`show[]`). The MACROS test in `tests/ncc_test.c` then checks every knob of every engine moves its
   parameter and its popup fits.
3. Categories: every factory sound must get one (the SOUNDS test says "none OTHER"). Add keywords to `NL_KW` in
   `ncc_library.c` as needed (e.g. ORGAN for WHEEL's GOSPEL / JAZZ PERC, CHOIR, BELL ..). Think about a new category
   **FX** for NOISE's WIND / RAIN / ARCADE / METAL (insert before OTHER in `NC_*`/`NC_NAME`; ask Remco). PHYS's HAND
   DRUM / TOMS fall under BEAT (keywords DRUM, TOM).
4. Many good NCC patches per engine in `NCC_SND` (`ncc_presets.c`; format: engine, name, E0..E7, ADSR, ENV->FLT, mono,
   sends, LFO rate / SHP / FLT, Juno chorus | drift). The NCC SOUNDS test plays each one (not silent, not clipping).
   Aim for a useful spread per category; names that say what they are.
5. Check CPU (PHYS and GRAIN cost more: `fx.c ENG_VCOST`), the regression (factory goldens must not change), the
   emulator, then update `docs/MANUAL.md` (engines table, sound counts), README, site features and `CHANGELOG.md`.

## Open points (small)
- The GLIDE ring on the SOUND page shows the time wrongly; the Juno sound's name is lost after a power cycle (it shows
  "JUNO"); the clock comes from USB only (not TRS MIDI) in NCC's setup.
- `nc/ci`: a local branch with a GitHub Actions workflow; pushing it needs `gh auth refresh -s workflow`.
- UBSan reports an out-of-bounds read in `eng_slice.c:134` during the user-loop tests (stub slot data); look at it
  before LOOP comes back.
- LOOP (engine 15) is done but hidden (`NCC_LOOP 0`): user loops from the sample slots work, the manager's Loops tab
  appears when the device offers LOOP. Remco wants it back later, polished.
- Ideas Remco liked (from other FM-1 firmwares): a rhythmic gate per part on the clock, chords / scale on the FM-1's
  keys (+ strum), sound changes on the bar + favourites, freeze / infinite space, knob motion recording, an acid arp
  for ACID, sidechain duck, a safe mode, LOOP back.

## Remco's Mac (how it is set up)
- Apple silicon, the repo in Dropbox: `~/Library/CloudStorage/Dropbox/ClaudeCode/FM1/fm1-nc-companion`.
- Firmware builds: **Colima** with Rosetta (`colima start --vm-type vz --vz-rosetta`), no Docker Desktop; the JieLi
  toolchain in `~/.jieli/toolchain`, the SDK in `~/fw-AC79_AIoT_SDK`; builds run in the mirror `~/.cache/ncc-build`.
- Pythons: `/Library/Frameworks/Python.framework/Versions/3.13/bin/python3` (Pillow, fontTools: generators, builds);
  `/opt/homebrew/bin/python3` (mido, python-rtmidi: the installer). `python3` on PATH is Homebrew 3.15.
- Emscripten from Homebrew (`emcc`) for the emulator; `gh` logged in as fenderberg (no `workflow` scope).
- ASan does not start on this macOS: the tests use UBSan.
- The FM-1 shows up as MIDI port "X0X FM-1".
