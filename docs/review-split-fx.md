# Review brief: SPLIT effects (`nc/split-fx`, merged into main)

For the assistant reviewing this work. Read **AGENTS.md** first (chip rules, layers, commands), then this. The
owner is Remco: answer him in **Dutch**, plainly (he is a musician, new to firmware). Code, comments and docs stay
English.

## What is asked

Three things, in this order:

1. **A check of the code**: correctness first, in the audio ISR above all.
2. **Efficiency**: CPU on the AC791N and flash/RAM, without changing the sound.
3. **Ideas for new features**, now that there is flash to spare again.

**Report findings first; change nothing without Remco's OK.** Rank findings by severity and say for each what you
saw, the concrete failing case, and how sure you are. Keep proposals small and separate, so he can pick them one by
one. Never flash, push, merge into main or publish without his explicit OK each time.

## What the branch does

`git diff 798e690 50804ba` (0.7 to the split-fx commit, now on main; the branch `nc/split-fx` points at the same
commit). Two changes:

1. **SPLIT keeps the parts fully apart, effects included.** SPLIT (NCC's default) sends part L to the left output
   and R to the right, for the Circuit Tracks' two audio inputs. Until now the chorus, delay and reverb were shared
   buses whose returns came out on both sides. Now each side has its own chorus, delay and reverb, with one set of
   settings. The limiter is off in SPLIT (a loud L ducked R; the Circuit has its own limiter). STEREO is unchanged,
   bit for bit.
2. **LOOP's built-in loops are left out of the image** while LOOP is hidden (`NCC_LOOP 0`): 154 KB of flash freed.

Remco's choices (keep them): shared settings with separate signals; delay "A" (the same sound, each side's line
half as long, too-long times halved on the beat); reverb "thinner but separate" rather than off; limiter off in
SPLIT. **He listened on the device: "klinkt goed".**

## Where to look

| File | What changed |
|---|---|
| `firmware/src/ncc_split_fx.c` | **New, the core of the review.** One generic implementation of the buses for both layouts: chorus/delay (`ncs_chodly`), ROOM/SPRING/HALL (`ncs_room`, `ncs_spring`, `ncs_hall`), per-block values (`ncs_rev_k`), layout and model switches with a one-block fade (`ncs_block`, `ncs_buses`, `ncs_fade`), R's sends (`ncs_snd`, `NCS_R`), throws (`ncs_throw`), PERFORM's THROW (`ncs_pre`). Its header comment explains the layout. |
| `firmware/src/fx.c` | Small hooks marked `/* NC: */`: the include, `lim_off` in `master_out`, `ncs_begin` / `ncs_pre` / `ncs_buses` in `mix_block`, `ncs_throw` in `mix_part`. Felucca's `fx_buses`, `rev_room/spring/hall`, `rev_switch` and `hall_gains` are no longer referenced in NCC builds, so they are not built (that is how the code fits). |
| `firmware/src/juno_chorus.c` | `ncc_mix`: R's sends into `ncs_snd` in SPLIT. |
| `firmware/src/eng_loop.c` | `loop_src`: no built-in loop under `NCC_LOOP 0`, so `NLOOP_DATA` is unreferenced and dropped. |
| `tests/ncc_test.c` | `split_test`, `split_side_silent` (now expect exactly 0 on the other side, also with sends, throws and THROW), new `split_fx_run`, `split_echo_at`, `split_fx_test` (every model x side, delay halving, no limiter, SPLIT<->STEREO while ringing), `loop_none_test`. |
| `tools/ncc_dev.sh`, `tests/run_tests.sh` | The NCC regression builds with `-DNCC_LOOP=1`, so LOOP's 7 goldens keep guarding it. |

### Points that deserve a careful look

- **STEREO bit-identity** rests on the 113 NCC golden renders (`sh tools/ncc_dev.sh test`: "0 changed"). A
  deliberate 1-sample chorus change broke 53 of them, so they cover this path. Still worth reading against
  `fx_buses` / `rev_*` in `fx.c` (they are still in the file).
- **Buffer bounds in SPLIT**: two sides in the halves of `cho_buf` (2 x 1024), `dly_buf` (2 x 32768), `rev_comb`
  (2 x 2468) and `rev_u` (2 x 498 / SPRING's int32 states). Static asserts guard the lengths; check every index,
  especially SPRING's gliding length (`ssz`) after a layout change and HALL's mirrors and drifting taps.
- **The second core**: R may render on core 1 (`mix_part` -> `ncc_mix` / `ncs_throw` write `ncs_snd`); core 0
  reads after `C1_WAIT`. One array for both cores (R renders on one core per block). Known and accepted: if core 1
  stops answering mid-block, its parts are rendered again on core 0 and R's sends of that one block count twice.
- **Switches**: a SPLIT<->STEREO change fades the old layout for one block, clears chorus/reverb at once and the
  delay line over 32 blocks (4 KB a block, buses silent ~23 ms). A reverb model change fades the old model as
  Felucca's `rev_switch`. Check the combinations (a change during the clear, a model change in the first block).
- **Chip rules** (AGENTS.md): no float, no 64-bit division, guard every divisor, nothing slow in the ISR, one writer
  per shared variable. `ncs.split` is written by core 0 at the block start and read by core 1.

## Numbers

- **Flash**: XIP region 581564 B (`firmware/app.ld`). Image now **427100 B** (154 KB free). Before the loops were
  dropped the split FX fitted with only 48 B to spare: keep an eye on size anyway. RAM 62504 / 73728 B, pool
  359816 / 368640 B.
- **CPU, Mac host only** (instruction counts, not the AC791N): SPLIT costs +177..+195 instructions/sample over
  STEREO, for every model, idle or with 8 FM6 notes (about one FM6 voice). STEREO costs ~+18/sample over main in an
  empty mix, mostly the reverbs reading their lengths from tables instead of constants (ROOM ~+11, HALL ~+8,
  SPRING ~+6 per sample in an isolated benchmark). That puts a **third** entry over budget in the Mac regression
  (`cpu/mix/idle` 369 vs baseline 286; the two others, ANALOG/SINE_KEY and PHASE/BRASS, are known Mac noise). The
  baseline was not changed on purpose.
- `tests/target_budget.py` on an NCC build reports `rev_room/rev_spring/rev_hall` "not found": expected (not built
  in NCC); the plain Felucca build keeps them. On main its NCC numbers were already over the Felucca budgets.
- Not measured yet: the device's own load. The console `status` command reports `cpu_pct` and `audio_max_us`
  (`firmware/src/console.c`); compare SPLIT and STEREO with heavy sounds and HALL.

## Efficiency: starting points

- Win back STEREO's ~18 instructions/sample without losing bit-identity or growing the image much (for example
  specialised loops for the two layouts where it pays, or hoisting more per-block work).
- SPLIT's cost: reverb twice per block is the bulk. Anything cheaper that sounds the same is welcome; anything that
  changes the sound needs Remco's ears.
- Flash: is there code or data left that NCC never uses (Felucca features outside NCC's picker: DRUM, SLICE,
  DIGITAL, the sequencer UI)? Report what it would free; removing things is Remco's call.

## Features: context for ideas

NCC is a live companion to a Novation Circuit Tracks: the Circuit plays the notes and sends the clock, the FM-1's
panel is for picking sounds and jamming, **no sequencer**, "less to learn, more to play", and live tweaking without
glitches comes first. Ideas Remco liked earlier (`docs/HANDOFF.md`, Open work): rhythmic gate, chords/scale and strum
on the panel keys, changes on the bar / favourites, freeze / infinite space, motion recording, acid arp, sidechain
duck, safe mode, LOOP's return. Saving sounds on the panel shipped in 0.7. A Reddit user asked for user presets and
performances (two sounds plus effects), and liked that NCC is not menu-heavy.

For each idea: what it does for a player, which buttons/knobs (the panel is nearly full; see the button table in
`docs/MANUAL.md`), the cost in flash/RAM/CPU, and what it would break or complicate. Fewer, well-argued ideas beat a
long list.

## How to verify anything you change

```bash
sh tools/ncc_dev.sh test       # must say "ncc: all ok" twice, 113 goldens "0 changed"
sh tools/ncc_dev.sh build      # check the image size line; the linker refuses more than 581564 B
sh tools/ncc_dev.sh emu-test   # the WebAssembly panel/MIDI integration
```

Add a test for every new behaviour in the style of `tests/ncc_test.c`. Record what you did in `docs/HANDOFF.md`.
Report honestly what was tested on the host, on the device, or not at all.
