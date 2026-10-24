# Handoff: where NCC stands

Read **AGENTS.md** first (chip rules, architecture, code map and commands). Updated: 2026-10-24.

## Current source, installed firmware and public release

- Current development source: **twelve engines, 161 built-in sounds**, independent L/R parts, note-only Dual,
  STACK/REPLACE chord drones, SCL/GLO/EDIT controls and zero delay/reverb sends on sound selection.
- Installed on Remco's FM-1: development identity **FM-1_700**, package SHA-256
  `f5b0b041a7906ed590e13e4c44fad83316cf337f1460da8e32885ee6e58f3ec2`.
  Image **576728 B**, RAM **61704 / 73728 B**, pool **359464 / 368640 B**.
- Remco authorized the latest installation with “de replace drone is nu iets te plotseling. die mag vloeiender
  overgaan. doe dat en zet alles naar de fm1”. The installer completed the write/restart; separate identity and
  read-only NCC_INFO/LIB/PARTS checks passed. Twelve engines, **161 built-ins / 193 entries including 32 bank voices**.
- Existing state restored: PHASE/BELL on CH3, JUNO/SOFT PAD on CH4, SPLIT, TRS clock, 121 BPM, Dual off;
  R remembers REPLACE with Drone off. No notes, settings or sound loads sent during post-flash verification.
- Logs (local, ignored by git): `build/ncc-flash-smooth.log`, `build/ncc-flash-smooth.sha256`,
  `build/ncc-device-check-smooth.json`, `build/drone-smooth-test-run.log`, `build/drone-smooth-emu-run.log`.
  Earlier installs: first Dual build 570076 B, SCL/chord build 573408 B; superseded by the package above.
- **Public release remains 0.3** (`FM-1_703`): five engines / 81 sounds, sound filters and preview, pulse-locked arp,
  LOOP hidden, complete DX7 bank names. The website/installer still serves that release:
  https://fenderberg.github.io/fm1-nc-companion/ . The current README/manual/changelog describe development explicitly.
- Remco authorized committing/pushing all pending source changes and updating documentation. This source sync
  does not create a release, deploy the site or authorize another flash. Later actions need their own explicit OK.

## Performance controls and state

- Normal notes: L channel 3, R channel 4; SPLIT by default. OCT-/+ select which part the panel edits.
- **SCL** (small button beside FX) toggles Dual using the remembered **CH3/CH4** source; **GLO page 2** chooses it.
  Only that physical channel supplies notes to both existing parts; local keys play both. Macro CCs, bend,
  aftertouch, sustain and Program Change keep their usual per-part targets. Shared FX retain their existing buses.
  Switching Dual/source fades current notes and clears held input. GLO + HOME is ordinary HOME.
- **SEQ** toggles Drone on the focused part. ARP page 2, knob 4 chooses STACK/REPLACE; also available under GLO/Manager.
  STACK keeps its toggle behavior. REPLACE groups all notes until every physical key is released; the next strike
  replaces the whole chord. Sustain does not extend the physical gesture; CHORD expansion counts as one strike.
  Use POLY for simultaneous tones; MONO/LEGATO/UNISON and the existing shared voice cap retain their own behavior.
- REPLACE now uses **70 blocks (~50.8 ms) out, then 70 in**. First strikes start next block at unity. Old voices render
  until a zero-gain block has completed, then their slots are reused and the queued chord fades in. Fast gestures
  replace the queue without restarting fade-out; a reversal during fade-in starts at the current gain. No extra
  voices/sample buffers. Patch envelopes remain intact. Panic/Stop/Drone off/routing changes cancel safely.
- `ncc_drone_fade.c` is included by voice.c; the event core alone updates gains before render cores read them.
  `ncc_seq.c` owns the bounded 16-tone queue. `ncc_input.c` uses private MIDI ownership slots 16=L/17=R and per-part
  local-key snapshots. Old note-offs, pedal ownership and focus/chord changes cannot strand or steal another layer.
- **GLO**: two pages, SELECT/GLO cycles. Page 1 channels/output/clock; page 2 Dual/BPM/L Drone/R Drone.
  **EDIT**: engine descriptors, envelopes, LFO, voices and chord; JUNO filter and FM6 operator/envelope/mode pages.
  FM6 edits use the existing patch writer and OWN slot. Sound edits support undo and preserve intentionally wet sends.
- Every newly selected NCC/factory/DX7/user sound starts with P_DLY=P_REV=0; the other part and shared tails remain.
  Restore/undo keep saved wet values. `ncc_sound_defaults.c` handles the outermost load, including initialization/PC.
- No stored-format/engine-ABI change. G_SYNC holds marked Dual/Drone bits (see ncc_settings.c and protocol docs);
  old/corrupt values mean OFF/STACK. Dual source/modes persist; Drone on/off and sounding notes do not.
- Manager protocol 96..100 has append-only INFO/PARTS trailers and SET operations 4/5. Legacy replies remain usable.
  The Manager mock now offers representative sounds from all twelve engines and exercises library pagination.
  Full wire descriptions: `web/EDITOR_PROTOCOL.md`, implementation comments in editor_ncc.c.

## Engines and sound library

- Engine order: FM6, JUNO, PHASE, TRIO, ACID, **PHYS, GRAIN, VOICE, LOFI, WHEEL, NOISE, SAMPLE**.
  SELECT remains category, ALGORITHM engine, PRESETS sound; none wraps. LOOP remains hidden (`NCC_LOOP 0`).
- The seven additions have all eight Circuit macro slots (CC 1,2,5,11,71,74,12,13), same base/middle and slew.
  `ncc_extra_macros.c`: PHYS model labels; LOFI DUTY becomes SWP on triangle/saw/noise, AMP ENV becomes RELEASE in
  STEP. Destination changes preserve knob/slew ownership. WHEEL FILTER ENV is percussion (no cutoff input),
  SAMPLE RESONANCE is drive and FX is chorus. Tables: `docs/MANUAL.md`, `docs/NC-extra-engines-macros.md`.
- `ncc_extra_presets.c`: **46 patches** (8 each PHYS/GRAIN/VOICE, 6 each LOFI/WHEEL/NOISE, 4 SAMPLE), plus 34 existing
  factory sounds. Total **161**: 25 BASS, 40 PAD, 36 LEAD, 29 KEYS, 27 PLUCK, 4 BEAT. No new category or external
  samples/banks. Peak checks pass for every NCC patch; musical quality still needs listening.
- Library capacity 384 (previously 192), with compile reserve/runtime full-bank-user checks, including room for 48
  future loops. Extra engines get category fallbacks for unknown names; Manager keyword previews match firmware.
- `ncc_formant_math.c`: exact first-formant scaling with 32-bit divides; 100000 comparisons against the old quotient.
  `ncc_noise.c`: local renderer with wide products for colored noise poles, color/filter/gain calculations (RADIO
  PAD exposed signed overflow). Upstream constants/presets/descriptors reused; engine submodule/ABI untouched.
  Non-NCC code still uses the original renderer. All existing golden hashes remain unchanged.

## Other fixes included

- Manager PARTS tempo encodes both BPM bytes from the same 0..300 value (127/128 and 255/256 tests).
- `ncc_sample_data.c` resolves validated built-in/user XIP addresses with integer arithmetic, avoiding indexing a
  separate allocation through SMP_DATA. Shared by SAMPLE/GRAIN/SLICE, LOOP decoding and waveform display; no sample
  format/audio change. Tests compare all built-in/user bytes and decoders through every slot. Host fixture images
  must still lie above SMP_DATA within 4 GiB because rebased offsets are 32-bit.
- `tools/ncc_check.py` preserves exit failures/crashes/missing summaries and writes full logs. ncc_dev.sh uses it
  for test/build/release. Both NCC variants use fatal UBSan; **only shift-base is excluded** for existing signed DSP
  left shifts. Bounds/division/overflow/invalid counts remain fatal. Six runner tests cover failure paths.

## Validation and its limits

- `sh tools/ncc_dev.sh test`: both NCC variants pass (`ncc: all ok`), Manager/editor/runner pass; **113 NCC + 98 plain
  golden renders unchanged**, no golden/baseline updates. Two known Mac CPU differences are accepted only for
  ANALOG/SINE_KEY and PHASE/BRASS, at most +35%, with no health/routing/crash failures.
- Tests cover real CC isolation in Dual, all **56 additional macros changing rendered audio**, all **49 extra-engine
  pairs** under live CCs, PHYS/GRAIN caps, full library, sample data, settings/edit/undo, chord ownership and longer
  fades including zero-boundary replacement, reversal and cancellation. `tests/ncc_extra_cpu.c` records host-only
  comparisons in `build/host/ncc_extra_cpu.log`; these do not certify AC791N deadlines.
- `sh tools/ncc_dev.sh emu-test`: real Wasm panel/MIDI paths for settings/edit, chords, Dual, every extra engine and
  macro; old drone still renders 20 ms into replacement. Audio bounded. Screens in `build/emu-demo/performance*.png`.
- `sh tools/ncc_dev.sh build`: succeeds with the memory figures above. Device write/restart and library/protocol
  availability checked. **Listening, physical panel/Circuit use and hardware timing remain practical tests.**
- The broader Felucca suite was run earlier with `SANITIZE=0`, outside Dropbox. Known Mac failures: font spacing,
  PERFORM/regression CPU budgets, two always-built DRUM ASan processes aborting during initialization. Other groups
  pass; optional sanitizer/fuzz phase skipped. Logs: `build/host/full-suite.log`, workdir in
  `build/host/full-suite-workdir.txt`. Not rerun for each later panel/engine/fade follow-up.

## Open work

- GLIDE ring time on SOUND is wrong; an NCC patch's own name is lost after power cycle (JUNO falls back to JUNO).
- Audit signed DSP left shifts before removing the UBSan shift-base exception.
- LOOP/user loops need device polish/testing before being offered again. DRUM/SLICE/DIGITAL remain outside NCC's picker.
- Clock is selectable INT/USB/TRS in GLO and Manager; USB is the default. The older “TRS unavailable” note is obsolete.
- Local branch `nc/ci` has a GitHub Actions workflow; publishing workflow changes needs a token with workflow scope.
  It is separate from the current source sync; do not push unrelated branches.
- Ideas Remco liked: rhythmic gate, chords/scale and strum on panel keys, changes on bar/favourites, freeze/infinite
  space, motion recording, acid arp, sidechain duck, safe mode, LOOP's return. These are proposals, not shipped controls.
- Device-only content (not in git): DX7 bank BANK0054 (32 voices), USR1 LOOPS (3 loops). Do not ship copyrighted
  recordings/ROM banks; the local best-of DX7 file under build/dx7 is not a repository asset.

## Working environment

- Owner Remco / GitHub Fenderberg; answer Dutch, code/docs/comments English except Dutch design records.
- Apple silicon Mac; repository in Dropbox. Colima with Rosetta, toolchain `~/.jieli/toolchain`, SDK
  `~/fw-AC79_AIoT_SDK`; builds in `~/.cache/ncc-build`. Setup: `sh tools/setup_env.sh`.
- Framework Python 3.13 for generators/builds (Pillow/fontTools); Homebrew Python for mido/python-rtmidi.
  Paths/overrides in tools/ncc_dev.sh. Emscripten for emulator, Node for Manager/integration tests.
- MIDI port X0X FM-1. Use tools/ncc_dev.sh for test/build/emu/manager/info/flash/site tasks. Never reflash or publish
  on the basis of a previous consumed authorization; source push authorization covers this source sync only.
