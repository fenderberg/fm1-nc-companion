# Handoff: where NCC stands

Read **AGENTS.md** first (chip rules, architecture, code map and commands). Updated: 2026-10-25.

## Main and website sync (2026-11-01)

- Remco authorized "push deze versie naar de main. update de documentatie en de website". Added short MANUAL.md
  lines for the new preset/engine-switch arp and drone behavior (Arp and HOLD, Drone sections). Committed and
  pushed source commit **4163974** to `main`: the DX7 bank Mac build fix, the corrected `arp_seq_notes_test`, the
  preset/engine-switch arp and drone feature with its test, and `.gitattributes`.
- `NCC_NO_SERVE=1 sh tools/ncc_dev.sh site 0.4` built the site from the unchanged, already-published 0.4 package
  (checksum verified against the official release, unchanged). The new changelog line appears under the home
  page's **Next** section; the manual's Arp/Drone sections carry the new text. Published to `gh-pages` commit
  **866f2b3**. Live checks (`curl`, after allowing a short CDN propagation delay) confirm the home page's new Next
  line, the manual's new text and the live firmware download's checksum, all matching the build. No new release,
  no additional device flash (the FM-1 was already updated earlier in this session, recorded above).

## Installed on Remco's FM-1 (2026-11-01)

- Remco authorized the update with "update fm1". Package `build/ncc.fwsc`, identity `FM-1_700`, image **578736 B**,
  RAM **61896 / 73728 B**, pool **359464 / 368640 B**, SHA-256
  `9e28adfda10372d91443c3bad6c4da563430c8e10890861641c227b16c210fd1`. This carries the preset/engine-switch arp and
  drone fix below, the DX7 bank-replace/remove fix and the Mac build fix for it (both from the earlier 2026-10-27
  work, now compiling and host-tested), and the test-only arp-grid investigation from this session. The arp clock
  lock, REPLACE overlap and PLAY-page navigation already installed on 2026-10-25 are unchanged by this update.
- `sh tools/ncc_dev.sh flash` completed: writing reached 100%, the FM-1 restarted and `sh tools/ncc_dev.sh info`
  confirms it is running `FM-1_700`. No deeper NCC_INFO/LIB/PARTS protocol check, no notes/CCs/settings were sent
  and no device listening test was done this time; identity and the write/restart outcome are what was verified.
  Not committed, not pushed, not released.

## New: a preset or engine switch keeps the arp and a plain drone going (2026-11-01, uncommitted, host-tested only)

- Remco asked: "nog 1 extra feature: behoud de arp als ik van presets switch" (keep the arp going when I switch
  presets), confirmed in scope to cover ALGORITHM (engine) switches too and both the ARP and Drone-hold (SEQ).
- Until now, picking a new PRESETS sound, a new ALGORITHM engine, SAVE-held UNDO/REDO of a sound, or loading a user
  preset silenced the part and dropped everything it was holding (`panic_req`, seq.c `events_block`): a held arp
  stopped and had to be re-struck, a droning chord went silent for good.
- Fix: a new `ncc_sound_req` bit (seq.c, set alongside `panic_req` at its four UI/upreset.c sound-change sites, and
  explicitly cleared at the two real "every track" panic sites in main.c and project.c) marks a panic as a sound
  change rather than a real reset. `events_block` still releases the currently sounding voice and its arp-note
  bookkeeping (`trk_all_off`, `arp_forget`) either way, but for a sound change it no longer calls
  `midi_forget_track` or clears `t->nheld` / `t->held[]` / `t->arp_phys`: the arp (already stepping from
  `t->held[]`) simply continues on the new sound at its next step. Without the arp, a droning chord's notes are
  replayed at once by a new helper, `ncc_sound_kept` (ncc_seq.c): it skips if a fade/overlap/pending REPLACE
  transition is already under way, so it never fights that machinery. `trk_note_on` (used for the replay, same as
  the engine-switch crossfade's own queued-note replay) already defers correctly through an actual engine change
  (voice.c `engine_block`), so no new crossfade logic was needed. Panic, OTA and project load still clear
  everything, unchanged.
- New test `preset_keep_arp_test` (tests/ncc_test.c): a held note's arp survives a same-engine preset switch and a
  full engine (ALGORITHM) switch and keeps sounding on the new sound either way; releasing the key still stops it
  normally; a plain (non-arp) drone note is replayed at once on a new preset; a real Panic still clears a held arp
  as before (regression). All pass; `sh tools/ncc_dev.sh test` is clean (`ncc: all ok`), 113 NCC / 98 plain golden
  renders unchanged, no CPU regressions beyond the two established Mac exceptions, fatal UBSan clean.
- Firmware builds: `build/ncc.fwsc`, identity `FM-1_700`, image **578736 B**, RAM **61896 / 73728 B**, pool
  **359464 / 368640 B**. **Not flashed, not committed.** No device/emulator listening test done; this is host
  evidence only (arp timing, note continuity and voice state), not a confirmation of how it actually sounds live.

## Mac-side follow-up on the Reddit report and the arp investigation (2026-11-01, uncommitted)

- Remco shared a second Reddit reply (2026-11-01): after running a while, the unit "suddenly develops severe
  latency and loses MIDI clock sync and stable timing (only a reboot restores it)". Remco linked this to the arp.
  His FM-1 was connected but **no device flash was done this session**; everything below is host-only.
- The Windows-written changes from 2026-10-27 did not actually compile on the Mac: `fm6_bank_changed()`
  (fm6_vbank.c) calls `nl_bank_changed()` before `ncc_library.c` is `#include`d (via live_ui.c, later in the unity
  build), so the compiler saw an implicit declaration and then a conflicting static one. Fixed with a forward
  declaration in fm6_vbank.c (marked NC), matching the pattern other NC hooks use. With that fix,
  `sh tools/ncc_dev.sh test` compiles and the DX7 bank-replace/remove fix and the Manager changes from 2026-10-27
  test clean; `fm6bank_test.c`'s new `remove_test` also passes standalone (`cc tests/fm6bank_test.c`, "fm6bank: all
  ok"). The full `tests/run_tests.sh` suite (the two Mac-specific exceptions aside) was not rerun this session.
- `arp_seq_notes_test` (added 2026-10-27, never run) **initially failed** on the Mac. Chasing it down: the test
  itself had a bug, not `ncc_seq.c`. `arp_add` (seq.c) seeds `t->arp_idx` to its sentinel (0xFFFFFFFF) on every
  fresh gesture, before any attack happens; the test's detection (`trk[1].arp_idx != pidx`) counted that harmless
  reset as a step, and flagged it "off grid" because it lands on the note's own pulse, not the arp's. Excluding the
  sentinel from the comparison (ncc_arp_clock_test.c) makes the test pass **with `ncc_seq.c` unchanged** — short,
  long, legato and micro-step-late sequencer notes, both rates, both clock sources, swing and tempo changes all
  land exactly on the grid with no missed or stray attacks. I also tried two real changes to `ncc_arp_due`/
  `ncc_arp_phase` (keeping the pulse grid valid across a brief note gap, and later a stricter one-attack-per-slot
  rule) while chasing the false failure; neither was needed once the test itself was fixed, so neither is kept —
  `firmware/src/ncc_seq.c` has no diff from committed HEAD.
- Conclusion: the 2026-10-27 "drifts once the sequencer plays its notes" hypothesis did not hold up under host
  testing once the test's own bug was removed; the pulse-grid logic in `ncc_seq.c` checks out for everything this
  test throws at it (a few minutes of simulated play). The new Reddit report's symptom (severe, growing latency
  and lost clock sync after a while, needing a reboot) sounds like something else — a long-uptime issue (a leak,
  an unbounded counter, or a stall), not a quantization edge case, and this test's ~3000-pulse runs are far too
  short to reach it. **Still to do:** a long-duration host simulation (hours of simulated MIDI/clock traffic, not
  minutes) looking for anything that grows or stalls, and more detail from the reporter (how long before it
  happens, whether it needs the arp or Drone specifically, which engine/mode, USB or TRS clock).
- Firmware still builds clean with these fixes: `build/ncc.fwsc`, identity `FM-1_700`, image **578480 B**,
  RAM **61896 / 73728 B**, pool **359464 / 368640 B**. Not flashed.

## Reddit report: DX7 bank names, bank removal (2026-10-27, uncommitted, not yet host-tested in C)

- A user (Reddit, video with a Circuit Tracks) reported: an imported Dexed .syx "corrupts" and cannot be wiped;
  the Tracks' template CCs intermittently stop responding; no patch editing in the Manager.
- Cause of the first: `nl_bank_names` cached the bank's names keyed only on `fm6_bank_used`; a full bank over a full
  bank (0xFFFFFFFF both times) kept the old names/categories over the new voices. Fix: `nl_bank_changed()`
  (ncc_library.c), called from `fm6_bank_changed` (fm6_vbank.c, marked NC). The Manager gains **Remove the bank**:
  BEGIN + END without voices (CRC 0), already supported and documented by the existing protocol, so it also works
  on 0.4 firmware. Tests: `bank_replace_test` (ncc_test.c), `remove_test` (fm6bank_test.c), Manager mock test.
- Written on Windows without a C compiler: **only `node web/ncc/manager_test.mjs` was run (passes)**. Run
  `sh tools/ncc_dev.sh test` and `tests/run_tests.sh` (fm6bank) on the Mac before committing.
- Remco (2026-10-27): the arp keeps time with held keys but drifts once the Circuit's sequencer plays its notes. No
  cause found by reading ncc_seq.c (attacks follow the pulse grid; a fresh note waits for the next slot or plays at
  once within 1/8 step). Added `arp_seq_notes_test` (ncc_arp_clock_test.c): sequencer-like notes (short, long,
  legato, micro steps, note before/after its clock), 1/16 and 1/32, USB and TRS. **Not run yet** (Mac).
- Felucca's editor (web/editor.html) against NCC firmware, by reading: the firmware answers all its commands,
  including FM6_GET/PUT on track 0/1 (L/R patch editing). The editor skips the globals NCC repurposes (G_ENGGO "SET",
  G_SYNC "SYNC"). Not checked yet: its hard-coded 14-engine table against NCC's 16 (ACID = 14), and P_ED_FX (Juno
  character) exposed as an ordinary parameter. Not tried on a device or the emulator.
- Still open from the report: the CC dropouts (no cause found in the code yet; plan: a host stress test with a
  Tracks-like stream over USB and TRS, plus questions to the reporter) and FM6 patch editing (first check whether
  Felucca's editor, still on the site at `webapp/editor/`, works with NCC).

## Main and website sync (2026-10-25)

- Remco authorized pushing the accumulated changes to main and updating the website, preserving its existing
  text style: “push naar main. pas de ebsite aan. hou de (tekst)style aan die daar reeds gehanteerd werd.”
- The site retains its short English copy, layout and current 0.4 installer. Unreleased arp/drone/PLAY changes
  appear under **Next**, and the browser demo is explicitly marked as a preview of what comes next. The manual
  and cheat sheet distinguish PLAY navigation from 0.4's SELECT. Drone wording now states that overlap needs
  POLY/free voices; the home feature describes both released and upcoming behavior without promising overlap
  in the 0.4 download. No new release or device flash is included in this publication.
- `NCC_NO_SERVE=1 sh tools/ncc_dev.sh site 0.4` builds successfully. Generated-page checks pass for local links,
  preview/Next labels, the unchanged official 0.4 package SHA-256 and the one-page cheat-sheet PDF.
  Logs: `build/main-site-update.log`, `build/main-push-test.log`.
- Published: implementation/source commit **0e394df040a6d1f32cd316f6444fa7e1d9746642** pushed to **main**.
  Pages commit **f463d3f8c3175814fc7f6fe0cc11823933350d89** built successfully. All nine checked live files match
  the generated files byte for byte: home, manual, cheat sheet and PDF, installer, Manager, preview HTML/Wasm and
  official 0.4 firmware. The firmware still hashes to `075b9d84b3f7a29b6595debaf1ded6e6681342a6895f3e54f9302195bfcfcb77`.
  Evidence: `build/main-site-publish.log`, `build/main-site-live.json`. Fresh host tests pass, including both NCC
  variants, unchanged 113/98 golden renders, Manager/editor/runner and the two documented Mac CPU exceptions.
  Site: https://fenderberg.github.io/fm1-nc-companion/ . No additional device flash or GitHub release was performed.

## Uninstalled PLAY page navigation (2026-10-25)

- Remco accepted the proposed controls with “voer dar uit”. SELECT now only filters categories on SOUND and has
  no action on ARP/FX/GLO/EDIT. PLAY navigates those pages: short release moves forward; a 500 ms hold moves back
  once, with no repeat or extra forward step on release. Navigation clamps at either end. SOUND ignores PLAY;
  PLAY never starts/stops transport. GLO/EDIT/FX repeated presses retain their previous behavior; ARP is unchanged.
- Gesture state is main-loop only, with no blocking waits. Changing family/subpage/part cancels a pending press.
  Existing knob/page tests and emulator interactions now use PLAY. New tests cover all four families, SELECT
  isolation, short/long thresholds, delayed release, cancellation, bounds and millisecond-counter wrap.
- Host tests, both NCC variants with fatal UBSan, Manager/editor/runner and emulator integration pass. The real
  Wasm test checks SELECT isolation and long PLAY/release. All 113 NCC / 98 plain golden renders are unchanged;
  only the two established Mac CPU exceptions remain. Manual, screen hints, README and changelog are updated.
- Firmware build succeeds: `build/ncc.fwsc`, **FM-1_700**, image **578416 B**, RAM **61896 / 73728 B**,
  pool **359464 / 368640 B**. SHA-256 `6831f2a61d4f51c84f0b8ae5c675c2e516a16d6913ed88a41ead61eb794a818b`.
  Logs: `build/play-pages-test.log`, `build/play-pages-emu.log`, `build/play-pages-build.log`.
- At implementation time no device write, GitHub push or publication was performed; source/site publication is
  now recorded above. The FM-1 still runs the combined arp/
  REPLACE package below; its flash authorization was consumed. 303 slide/accent behavior is unchanged.

## REPLACE overlap and combined firmware update (2026-10-25)

- Remco reported that REPLACE still cut off notes and authorized fixing it and updating the FM-1: “daarnaast
  kapt de drone functie in replace mode nog steeds de noten af als ik nieuwe noten speel. fix dat ook en update
  firmware”. The combined build includes the arp clock correction below. No push/release/site authorized.
- Root cause: the earlier ~51 ms fade silenced the entire part before starting the new chord. In POLY, when
  enough engine/shared voice slots are free, REPLACE now retains the old voices for a ~250 ms smooth fade and
  starts the new chord with its own ~80 ms minimum entrance (longer patch attacks remain). Repeated pitches get
  separate voices. Existing retiring tails keep their deadlines during rapid gestures. Chord expansion queues
  if outgoing tails temporarily occupy the required slots. No sample buffers or engine ABI/state format changes.
- Mono/unison/arp or insufficient free voices use a ~250 ms out / ~250 ms in fallback. This still passes through
  silence; it cannot provide overlapping chords in a monophonic engine or when every voice is occupied. Engine
  caps (including PHYS/GRAIN's three) and the shared eight-voice budget are preserved. Per-voice event-core ramps
  live in `ncc_drone_overlap.c`; render cores only read them. Panic/Stop/Drone off cancel retiring voices safely.
- Longer sustained-note tests exposed signed overflow in FM6 phase modulation (`phase + in[i]`, also feedback).
  NCC now explicitly wraps phase addition through uint32_t, matching its existing phase increment. Boundary
  tests cover both signed limits. This resolves the FM6 finding recorded during the earlier arp investigation;
  no engine submodule changes and no golden audio changes.
- Host tests (both NCC variants, fatal UBSan), Manager/editor/runner and Wasm integration pass. All 113 NCC / 98
  plain golden renders are unchanged; only the two established Mac CPU exceptions remain. New tests cover
  rendered audio continuity, repeated pitches, triads, full capacity, cancellation and all twelve engines.
  In the steady JUNO fixture the lowest 32-block transition-window absolute sample sum is 3180300, against
  3175259 before replacement (no silent handover). This is host evidence, not a listening/deadline test.
- Combined development package `build/ncc.fwsc`, identity **FM-1_700**: image **578200 B**, RAM **61896 / 73728 B**,
  pool **359464 / 368640 B**. SHA-256 `f2eb255325eb756babbcec0556eeecc11644f8f73783650fa7516e880cdfa1b7`.
  Logs: `build/drone-overlap-test.log`, `build/drone-overlap-emu.log`, `build/drone-overlap-build.log`,
  `build/ncc-flash-overlap.log`, `build/ncc-device-before-overlap.json`.
- **Installed successfully on Remco's FM-1.** Installer completed writing/restart and verified running
  **FM-1_700**. Separate identity and read-only NCC_INFO/LIB/PARTS checks passed: twelve engines, 161 built-ins,
  193 library entries including the existing bank. `build/ncc-device-after-overlap.json` records the package
  checksum above. Channels 3/4, SPLIT, TRS clock, 120 BPM, Dual off and both remembered REPLACE modes survived.
  Drone on/off is intentionally not stored: both parts restart with Drone off. L's displayed name changed from
  ACID LEAD to ACID 303; R remains FM6/PAD (the known sound-name/category restore issue, not evidence of bitwise
  patch equality). No sound loads, settings, notes or CCs were sent during post-flash checks. Logs also include
  `build/drone-overlap-device-after.log`, `build/ncc-device-after-overlap.log`, `build/ncc-flash-overlap.sha256`.
  This consumes the flash authorization above. Hardware listening/Circuit timing remains for Remco to check.

## Unreleased arp clock correction (2026-10-25)

- Remco reported arp drift against the Circuit clock. Reproduced actual off-grid arp attacks on both USB and
  TRS with clock jitter and bar-by-bar tempo changes. The existing bar-position test passed while attacks drifted:
  only the bar counter followed pulse position; the separate arp sample timer lost block overshoot and did not
  rescale its phase with tempo corrections.
- NCC's two arps now schedule attacks from Q8 MIDI pulse position. Straight rates and triplets land on their
  exact incoming pulses; swing uses bounded interpolation inside a pulse. All ten rates, including 4BAR, share
  the clock grid. Late chord entry keeps the one-eighth-step grace window. Delayed packet bursts emit at most
  one catch-up attack per block. Internal-clock/fallback behavior and the plain Felucca build are preserved.
  MIDI Start also resets held arps during playback; Stop still drops held notes.
- `tests/ncc_arp_clock_test.c` checks actual step changes: sixteen bars of jitter/tempo changes per USB/TRS source,
  plus all ten rates with zero/full swing and repeated Start. Both sources now produce 256/256 expected straight
  steps with zero missed or off-grid attacks. `sh tools/ncc_dev.sh test` passes, including fatal UBSan and LOOP;
  all 113 NCC / 98 plain golden renders are unchanged. Only the two documented Mac CPU-noise exceptions remain.
- `sh tools/ncc_dev.sh build` passes: development `build/ncc.fwsc`, identity `FM-1_700`, image **577240 B**,
  RAM **61736 / 73728 B**, pool **359464 / 368640 B**. Package SHA-256:
  `6b909ebb0ad50d7b456502fe82ac8dbefec0b92cc91a3bcbf61191991d8f93c9`.
  Logs: `build/arp-clock-before.log`, `build/arp-clock-after.log`, `build/arp-clock-build-run.log`,
  `build/arp-clock-emu.log`. Emulator panel/MIDI checks pass; these are host simulations, not Circuit/FM-1
  hardware timing or listening checks.
- The original arp report did not authorize a flash. The later combined update above includes this correction;
  public v0.4 remains unchanged. Check timing by listening with the Circuit before publishing this correction.
- Separate DSP follow-up: the initial long clock stress on the default L/FM6 sound aborted in fatal UBSan
  (`fm6_op_run`, signed addition overflow; local macOS report `ncc_test-2026-10-25-095533.ips`). Timing tests use
  R/JUNO to isolate scheduling. Resolved by the explicit NCC phase wrapping in the combined drone update above.

## Previous installation and public 0.4 release

- Public 0.4 source: **twelve engines, 161 built-in sounds**, independent L/R parts, note-only Dual,
  STACK/REPLACE chord drones, SCL/GLO/EDIT controls and zero delay/reverb sends on sound selection.
- Previous installation on Remco's FM-1: development identity **FM-1_700**, package SHA-256
  `f5b0b041a7906ed590e13e4c44fad83316cf337f1460da8e32885ee6e58f3ec2`.
  Image **576728 B**, RAM **61704 / 73728 B**, pool **359464 / 368640 B**.
- Remco authorized that installation with “de replace drone is nu iets te plotseling. die mag vloeiender
  overgaan. doe dat en zet alles naar de fm1”. The installer completed the write/restart; separate identity and
  read-only NCC_INFO/LIB/PARTS checks passed. Twelve engines, **161 built-ins / 193 entries including 32 bank voices**.
- State restored at that installation: PHASE/BELL on CH3, JUNO/SOFT PAD on CH4, SPLIT, TRS clock, 121 BPM, Dual off;
  R remembers REPLACE with Drone off. No notes, settings or sound loads sent during post-flash verification.
- Logs (local, ignored by git): `build/ncc-flash-smooth.log`, `build/ncc-flash-smooth.sha256`,
  `build/ncc-device-check-smooth.json`, `build/drone-smooth-test-run.log`, `build/drone-smooth-emu-run.log`.
  Earlier installs: first Dual build 570076 B, SCL/chord build 573408 B; superseded by the package above.
- **Release 0.4** (`FM-1_704`) packages this code: twelve engines / 161 sounds, Dual, chord drones, GLO/EDIT and dry
  defaults. Website/installer: https://fenderberg.github.io/fm1-nc-companion/ . Release metadata differs from the
  installed development build; no synth logic was changed for packaging.
- Remco explicitly authorized the public release/webinstaller update with “doe dat” after being told publication
  was separate from main. This covers the release, its source documentation and website deployment. The device
  already ran the same firmware logic at publication, with its write/restart and library checks recorded above; no additional
  device flash is part of publication. Listening and hardware timing limitations below remain explicit.

## Release 0.4 publication

- Publication completed: GitHub latest release **v0.4**, tag target **dc707a05b09319b5f4b448db430d7b7c1c6caddd**.
  Pages commit **5dbe90353b3ad36faf6247d1b8bc20f3798a2d64** built successfully. Normal live URLs for the home,
  manual, installer, Manager/protocol and v0.4 demo were fetched and checked; the live firmware checksum matches
  the release exactly. All four uploaded asset digests match the local files. Evidence:
  `build/release-0.4-github.json`, `build/release-0.4-live.json`, `build/release-0.4-publish.log`.

- Release package: `ncc-0.4.fwsc`, identity `FM-1_704`; image **576724 B**, RAM **61704 / 73728 B**,
  pool **359464 / 368640 B**. The four-byte image difference from the installed development build is release
  metadata; firmware source is unchanged. SHA-256:
  `075b9d84b3f7a29b6595debaf1ded6e6681342a6895f3e54f9302195bfcfcb77`.
- Tag/release: https://github.com/fenderberg/fm1-nc-companion/releases/tag/v0.4 . Assets include the firmware,
  licence/app ZIP, complete source archive with engine submodule and download checksums. GitHub's automatic source
  ZIP alone omits submodule contents. The source archive uses the tagged tree and engine commit c46e0086.
- `sh tools/ncc_dev.sh release 0.4` builds committed HEAD. `NCC_NO_SERVE=1 sh tools/ncc_dev.sh site 0.4` builds the
  matching installer/Manager/manual/demo; the emulator identifies as v0.4 without a preview suffix. Pages publishing
  preserves its branch history via a normal push. The public site is built from that release package.
- Release validation: host/NCC/Manager/editor/runner tests and v0.4 Wasm integration pass; package identity, loader,
  bundle licences and checksums checked. Generated installer uses FM-1_704 and the exact release package; home and
  manual describe 0.4. Logs: `build/release-0.4-{tests,emu,build,site}.log`.
- At publication the device ran the previously installed FM-1_700, with the same firmware logic and the device checks above.
  This publication does not claim a new release-package flash or an audible/hardware-deadline test.

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
- REPLACE uses per-voice overlap in POLY when slots are free: **344 blocks (~250 ms)** for retiring notes,
  **110 blocks (~80 ms)** minimum entrance for the new chord. The capacity/mono/unison/arp fallback uses 344 blocks
  out, then 344 in. First strikes start next block at unity. Retiring voices render their zero endpoint before
  reuse. Existing budgets and patch envelopes remain intact; no sample buffers. Fast strikes preserve retiring
  deadlines; a fallback reversal starts at the current gain. Panic/Stop/Drone off/routing changes cancel safely.
- `ncc_drone_fade.c` is included by voice.c; the event core alone updates gains before render cores read them.
  `ncc_seq.c` owns the bounded 16-tone queue. `ncc_input.c` uses private MIDI ownership slots 16=L/17=R and per-part
  local-key snapshots. Old note-offs, pedal ownership and focus/chord changes cannot strand or steal another layer.
- **GLO**: two pages, PLAY navigates and GLO cycles. Page 1 channels/output/clock; page 2 Dual/BPM/L Drone/R Drone.
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
- The whole working copy (including `.git`) is shared via Dropbox between Remco's MacBook and a Windows PC. Work
  on one machine at a time, and start only after Dropbox has finished syncing; never run git or an assistant on
  both at once. `.gitattributes` keeps LF everywhere (Windows git has core.autocrlf=true; a CRLF script breaks
  on the Mac) and `core.fileMode false` in `.git/config` hides Windows' lost executable bits. Build, test and
  flash on the Mac: the Windows toolchain/test setup is not established yet.
- Apple silicon Mac; repository in Dropbox. Colima with Rosetta, toolchain `~/.jieli/toolchain`, SDK
  `~/fw-AC79_AIoT_SDK`; builds in `~/.cache/ncc-build`. Setup: `sh tools/setup_env.sh`.
- Framework Python 3.13 for generators/builds (Pillow/fontTools); Homebrew Python for mido/python-rtmidi.
  Paths/overrides in tools/ncc_dev.sh. Emscripten for emulator, Node for Manager/integration tests.
- MIDI port X0X FM-1. Use tools/ncc_dev.sh for test/build/emu/manager/info/flash/site tasks. Never reflash or publish
  on the basis of a previous consumed authorization; each later device flash or publication needs explicit OK.
