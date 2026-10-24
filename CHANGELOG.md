# What's new in NCC

## 0.4

Twelve engines, Dual sounds and smoother chord drones. Release identity: `FM-1_704`.

- Seven additional engines in the existing sound picker: PHYS, GRAIN, VOICE, LOFI, WHEEL, NOISE and SAMPLE.
  All eight Circuit Tracks macros have real audio destinations, with model-dependent LOFI controls and popup names.
- 46 new NCC patches; 161 built-in sounds in total. Expanded library capacity also fits full DX7/user/loop lists.
  The existing categories include physical percussion and noise textures; LOOP remains hidden.
- VOICE bandwidth scaling uses exact 32-bit divisions. NCC's NOISE renderer widens products that could overflow
  with colored shift-register sounds; the upstream engine submodule and non-NCC build are unchanged.
- Dual layers the existing L/R parts from one exclusive MIDI note source (CH3 or CH4), or the FM-1 keys. SCL turns Dual
  on/off, remembering CH3/CH4; GLO selects the source. Each part keeps its own sound, arp and controllers.
  Changing source fades current live notes.
- Drone has STACK and REPLACE modes per part. REPLACE holds a whole chord: add notes while any physical
  key is down; after all are released, the next strike fades and replaces the old chord. Sustain does not extend
  the gesture. Replacing a sounding drone now uses a smooth ~51 ms fade-out followed by a ~51 ms fade-in;
  the first strike remains direct and rapid gestures update the waiting chord. Repeated notes,
  MONO/LEGATO/UNISON and ARP HOLD retain their voice behavior. Select the mode on
  ARP page 2, KNOB 4; SEQ remains on/off.
- Every newly selected sound starts with delay/reverb sends at zero, including user sounds and DX7 bank voices.
  Sends can be turned up as usual. The other part, stored presets, shared bus settings and tails are preserved;
  complete project restore and undo restore their saved settings.
- GLO opens two settings pages with L/R MIDI channels, output, clock, BPM, Dual and both Drone controls.
- EDIT opens the focused part's engine parameters, envelopes, LFO, voices and chord settings, with JUNO filter and
  FM6 operator pages. Edits keep deliberately adjusted sends; changing an FM6 SLOT starts its sends at zero.
- The Manager offers Dual and Drone settings, with optional protocol trailers for compatibility with older firmware.
  Dual source and Drone modes persist in the existing project format; active Drone notes and on/off do not.
- The NCC Manager shows the correct tempo above 127 BPM.
- User sample reads use explicit XIP addressing, avoiding an invalid array access in memory checks while keeping
  the sample format and decoded audio unchanged.
- Host tests and firmware builds preserve failed exit codes and full logs; both NCC variants run with fatal UBSan checks
  (existing signed left shifts remain excluded pending a separate DSP audit).

## 0.3

- **Clearer sound picking.** SELECT is the category, ALGORITHM the engine, PRESETS the sound; all three stop at the
  ends instead of going round. The part's box shows its category and engine as two labels, the bottom line its place
  in the list, and while you turn you see where you're going (PRESETS: the sound's name before it loads).
- **The LOOP engine is coming soon**: it is left out of this version while it gets finished.
- **Locked to the Circuit's bar.** On the Circuit's clock, NCC now counts the bar in its clock pulses, so the arp can
  no longer drift off the Circuit over time. Press Stop and Start on the Circuit once after installing.
- A full 32-voice DX7 bank keeps its voice names and types after the FM-1 is switched off and on.

## 0.2

- **Sounds by role, not by engine.** SELECT picks the type (BASS, PAD, LEAD, KEYS, PLUCK, BEAT), ALGORITHM the
  engine (or ALL), PRESETS walks through what's left.
- **88 sounds**, 30 of them new: keys, plucks, bells, pads, basses and leads.
- **LOOP**, a new engine: lo-fi one-bar drum loops that always fit the bar, opened by any note, with shuffle,
  reverse, stutter, crush, a DJ filter and pitch on the Circuit's knobs. Seven built-in loops, and your own.
- **NCC Manager**, a web page over USB: sounds on L and R, your own sounds, DX7 banks, drum loops, backups,
  settings.
- FX opens on SENDS; delay and reverb work in SPLIT; the Circuit's knobs 5 to 8 fixed; the knob popup shows what
  each knob does on each engine.

## 0.1

- The first release: two parts on the Circuit's MIDI 1 and MIDI 2 tracks, split output, the Circuit's eight knobs
  mapped per engine, five engines (FM6, JUNO, PHASE, TRIO, ACID), a clock-locked arp with HOLD, Drone, FX pages
  and throws.
