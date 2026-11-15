# What's new in NCC

## Unreleased

- **SPLIT keeps the parts fully apart, effects included**: each side has its own chorus, delay and reverb (one set
  of settings for both), so L's echoes and tails never reach the right output and R's never the left. No limiter in
  SPLIT any more, so a loud part no longer makes the other quieter. In SPLIT a delay longer than 0.74 s plays at
  half its length (on the beat) and the reverbs are a little smaller. STEREO sounds as before.

## 0.7

- **Save your own sounds on the FM-1**: press SAVE, then tap SAVE to overwrite your sound or hold it for a new one. A
  new sound is named after its source ("SOFT PAD 2"), keeps its category and also shows under SELECT's new **USER**
  category (and in the Manager).
- **Each page button turns its own pages**: tap GLO, EDIT or FX again for the next page (after the last, the first
  again), hold it for the previous one (before the first, the last). HOME or ARP close FX. PLAY no longer changes
  pages.
- **KNOB 4 turns the ARP's pages**, as ARP itself switches the arp on and off. Its settings are now on three pages:
  MODE RATE OCTAVES, GATE SWING PROB, ORDER DRONE.
- **SELECT picks the category on every page**, also in FX or EDIT, just like ALGORITHM and PRESETS.
- **The Circuit's AMP ENV knob makes a sound longer and leaves its attack alone**: the release only, on FM6 the
  carriers' decay and release.

## 0.6

- **Newer FM-1 units now save**: units with the newer UC25HQ80 flash chip kept nothing after power-off.
- **Steadier TRS MIDI**: old bytes are no longer read twice (stray CCs and clock pulses), and the clock is timed
  on arrival, also while the FM-1 is busy with heavy sounds and many knob moves.
- Manager requests no longer get lost now and then.
- A part that goes back to FM6 plays its latest patch, not an old copy.
- These fixes come from Felucca (Hügelton Instruments).

## 0.5

- **PLAY turns the pages** of ARP, FX, GLO and EDIT: tap for the next, hold for the previous. SELECT is only the
  category, on SOUND.
- **REPLACE drones crossfade** from one chord to the next in POLY, when voices are free.
- The arp stays exactly on the Circuit's clock, also when the tempo changes; Start restarts a held arp.
- A rare overflow in FM6 fixed.
- **A new DX7 bank shows its own names**: after a bank replaced another, the list kept the old names and categories.
- The Manager can **remove the DX7 bank**.
- **A preset or engine switch keeps the arp and a plain drone going**, instead of silencing the part.

## 0.4

- **Twelve engines**: PHYS, GRAIN, VOICE, LOFI, WHEEL, NOISE and SAMPLE join FM6, JUNO, PHASE, TRIO and ACID, all
  in the same sound picker and with the Circuit's eight knobs.
- **161 sounds**, 46 of them new.
- **Dual**: SCL lets one Circuit track (channel 3 or 4) play both parts, each with its own sound, arp and Drone.
- **Drone REPLACE**: hold a whole chord until the next one takes over. STACK works as before.
- **New sounds start dry**: no leftover delay or reverb from the sound before.
- **GLO** opens the settings, **EDIT** the sound's own parameters, down to FM6's operators.
- The Manager has the Dual and Drone settings, and shows tempos above 127 BPM correctly.

## 0.3

- **Clearer sound picking**: SELECT is the category, ALGORITHM the engine, PRESETS the sound, and all three stop
  at the ends. The part's box shows its category and engine.
- **Locked to the Circuit's bar**: the arp can no longer drift off over time.
- The LOOP engine is coming soon: it's left out while it gets finished.
- A full 32-voice DX7 bank keeps its names after switching off.

## 0.2

- **Sounds by role**: SELECT picks the type, ALGORITHM the engine, PRESETS the sound.
- **88 sounds**, 30 of them new.
- **NCC Manager**, a web page over USB: sounds, your own sounds, DX7 banks, backups, settings.
- FX opens on SENDS, delay and reverb work in SPLIT, knobs 5 to 8 fixed.

## 0.1

- The first release: two parts on the Circuit's MIDI 1 and MIDI 2 tracks, split output, the Circuit's eight knobs
  on five engines (FM6, JUNO, PHASE, TRIO, ACID), a clock-locked arp with HOLD, Drone, effects and throws.
