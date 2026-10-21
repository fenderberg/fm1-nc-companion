# NCC manual

NCC (Novation Circuit Companion) turns the M-VAVE FM-1 into a two-part sound module for the Novation Circuit Tracks.
The Circuit plays the notes and turns the knobs; the FM-1 makes the sounds and gives you the controls for playing live.

![NCC on the FM-1](ncc-emulator.jpg)

- [Hooking it up](#hooking-it-up)
- [The two parts](#the-two-parts)
- [The screen](#the-screen)
- [Sounds and engines](#sounds-and-engines)
- [The Circuit's 8 knobs](#the-circuits-8-knobs)
- [Arp and HOLD](#arp-and-hold)
- [Drone](#drone)
- [Effects (FX)](#effects-fx)
- [Split or stereo](#split-or-stereo)
- [Button cheat sheet](#button-cheat-sheet)
- [Installing](#installing)
- [Tips](#tips)

## Hooking it up

**MIDI.** Send the Circuit Tracks' MIDI to the FM-1's USB MIDI port (through a USB MIDI host adapter, or your computer
passing the Circuit's MIDI on). NCC listens on:

| Circuit Tracks track | MIDI channel | FM-1 part |
|---|---|---|
| MIDI 1 | 3 | **L** (FM6 to start with) |
| MIDI 2 | 4 | **R** (JUNO to start with) |

Use the Circuit's **factory MIDI template** on both MIDI tracks: its 8 macro knobs then send exactly the CCs NCC
expects (1, 2, 5, 11, 71, 74, 12, 13). NCC follows the Circuit's **clock and Start/Stop** over USB; with no clock
coming, the arp runs at the FM-1's own BPM.

**Audio.** Run a stereo-to-2×mono Y-cable from the FM-1's output into the Circuit's **audio inputs 1 and 2**. By default
part L is on the left channel only and part R on the right only, so each part arrives on its own Circuit input (see
[Split or stereo](#split-or-stereo)).

Then program notes on the Circuit's MIDI 1 and MIDI 2 tracks, press Play, and turn the Circuit's knobs.

## The two parts

The FM-1 has two parts, **L** (blue) and **R** (pink). Each has its own engine, sound, arp, drone and sends.

- **OCT-** puts the focus on **L**, **OCT+** on **R**. The FM-1's keys, its 4 knobs and its buttons always act on
  the part in focus. This is the quick way to jump between your two sounds while jamming.
- The Circuit always plays both parts, whichever has the focus.
- Up to 8 voices are shared between the two parts.

## The screen

- **Top bar:** the page (SOUND, ARP, or an FX page), `SPLIT` or `ST` (stereo), and the tempo (amber when it follows
  the Circuit's clock).
- **The two part boxes:** channel, engine and sound name of L and R. The part in focus is highlighted. A row of dots
  below shows the steps of the bar.
- **KNOB 1–4:** what the FM-1's own four knobs do on this page, with their values.
- **Bottom line:** a hint for the page, or a popup. When you turn a Circuit knob it shows the Circuit's label and
  what it does on that engine with the live value, for example `R FILTER FREQ   CUTOFF 2.1kHz`.

## Sounds and engines

On the **SOUND** page (press **HOME**):

| Control | What it does |
|---|---|
| **PRESETS** (knob) | Picks a sound for the part in focus. It loads once you stop turning, so you can scroll freely. |
| **SELECT** (knob) | Picks the engine for the part in focus: FM6, JUNO, PHASE, TRIO, ACID. |
| **ALGORITHM** (knob) | FM6: the patch slot (F1–F8 and your own DX7 bank). Other engines: the Juno chorus OFF / I / II / I+II. |
| KNOB 1–4 | LEVEL, OCTAVE, VOICE (poly / mono / legato ...), GLIDE |

The five engines:

| Engine | Character | Example sounds |
|---|---|---|
| **FM6** | 6-operator FM in the style of the DX7: e-pianos, bells, basses, brass. Loads DX7 banks (via Felucca's web editor). | TINE EP, BELL, FM BASS, BRASS, PAD, MARIMBA |
| **JUNO** | Analog-style poly synth with a Juno-style chorus and drift, plus a set of Juno sounds. | JUNO STRINGS, JUNO PAD, JUNO BRASS, POLY SYNTH, HOOVER |
| **PHASE** | Casio CZ-style phase distortion: bright, glassy, a bit raw. | BRASS, ORGAN, STRING, RESO, BELL, WIRE |
| **TRIO** | Three oscillators and a multimode filter, in the spirit of the C64's SID chip. | FAT BASS, ARP LEAD, SYNC LEAD, CHIP CHOIR |
| **ACID** | A 303-style bass: saw or square, a squelchy filter, accents on loud notes (velocity 100+), slides on overlapping notes. | ACID 303, SQUELCH, DEEP BASS, RUBBER, DUB ACID |

## The Circuit's 8 knobs

Each Circuit macro knob is mapped to what its label means on that engine. Knob in the middle (64) = the sound as
loaded; turn either way to move away from it. When you load a new sound, the knobs start again from the middle.
Moves are smoothed, so sweeps don't zipper.

| Circuit knob | FM6 | JUNO | PHASE | TRIO | ACID |
|---|---|---|---|---|---|
| 1 OSCILLATOR | operator ratios | osc mix (sub level on SUB) | waveform | osc 2 interval | saw / square |
| 2 OSC MOD | feedback | osc 2 detune (sync on SYNC) | detune | pulse width + detune | accent amount |
| 3 AMP ENV | carrier envelope times | attack + release | attack + release | attack + release | decay + release |
| 4 FILTER ENV | modulator envelope | envelope to filter | envelope to DCW | envelope to filter | env mod + decay |
| 5 FILTER FREQ | brightness | cutoff | DCW (brightness) | cutoff | cutoff |
| 6 RESONANCE | detune | resonance | wave fold | resonance | resonance |
| 7 MODULATION | LFO on brightness + a little vibrato | LFO on filter + rate | LFO on DCW + rate | LFO on filter + rate | slide time |
| 8 FX | drive + chorus send | drive + chorus send | drive + chorus send | drive + chorus send | drive + delay send |

If you change a value on the FM-1 itself while a Circuit knob is turned, your FM-1 setting stays, and the Circuit
knob keeps working on top of it.

## Arp and HOLD

- **ARP** switches the arp on for the part in focus and opens the **ARP** page. Press ARP again on that page to switch
  it off.
- KNOB 1–4: MODE (UP, DN, UPDN, RND, ORD, CHORD and more), RATE (1/4 to 1/32, triplets), OCTAVES, GATE. Turn
  **SELECT** for page 2: SWING, PROB, ORDER.
- The arp is always locked to the Circuit's clock. A new chord starts on the next step of the grid, so it stays in time.
- **REC** = **HOLD**: the arp keeps playing after you let go. A new chord replaces the held one; press REC again to
  release.
- The Circuit's **Stop** stops the arp.

## Drone

**SEQ** = **Drone** for the part in focus. With Drone on, notes keep sounding after their note-off: build up a pad
from the Circuit or the FM-1's keys and jam over it. Play a droning note again to let it go; press SEQ again to
release everything. The Circuit's **Stop** releases the drone too (Drone stays switched on for the next notes).

## Effects (FX)

**Tap FX** to open the effects. Each further tap of FX (or a turn of **SELECT**) goes to the next page; the tap after
the last page closes FX. **HOME** or **ARP** close it at once.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| **SENDS** (part in focus) | DRIVE | CHORUS send | DELAY send | REVERB send |
| **DELAY** | TIME (synced) | FEEDBACK | COLOR | MIX |
| **REVERB** | TYPE (ROOM / SPRING / HALL) | SIZE | DAMP | reverb SEND of the part |
| **CHORUS** | RATE | DEPTH | JUNO chorus of the part | DRIFT of the part |
| **INSERT** (part in focus) | TYPE (SOFT, HARD, FOLD, FUZZ, CRUSH, PHASER, FLANGER, CHORUS) | A | B | C (named per type) |
| **PERFORM** | FILTER | CRUSH | THROW | DEPTH |

While FX is open, the **white keys** play Felucca's PERFORM effects instead of notes: beat repeats (1/8, 1/16,
1/32), reverse, low-pass and high-pass sweeps, tape stop, freeze, octave shimmer, flanger, phaser and throws. Each
effect lasts as long as you hold its key. The PERFORM page shows which key does what.

**Throws, on every page:** hold **ENV** for an echo throw, hold **LFO** for a reverb throw.

## Split or stereo

- **SPLIT** (the default): part L only on the left output, part R only on the right. The dry sounds stay fully
  apart, so each part goes to its own Circuit input. The delay, reverb and chorus are shared by both parts, so
  their tails come out on both sides.
- **STEREO**: both parts in stereo on both outputs, for headphones or a single input.
- **GLO + SEQ** toggles between them; the top bar shows `SPLIT` or `ST`.

## Button cheat sheet

| Button / knob | Action |
|---|---|
| OCT- / OCT+ | Focus on part L / part R |
| HOME | SOUND page |
| PRESETS | Choose the part's sound |
| SELECT | SOUND page: engine. ARP / FX: next page |
| ALGORITHM | FM6: patch slot. Others: Juno chorus |
| ARP | Arp on + ARP page / off |
| REC | Arp HOLD |
| SEQ | Drone |
| GLO + SEQ | SPLIT ↔ STEREO |
| FX | Open effects, next page, close |
| ENV (hold) | Echo throw |
| LFO (hold) | Reverb throw |
| Keys | Play the part in focus (with FX open: PERFORM effects) |

Your parts, engines, sounds and effect settings are kept by Felucca's autosave.

## Installing

Installing firmware is at your own risk, but the way back is easy: M-VAVE's own updater, or Felucca's
[web installer](https://hugelton.github.io/Felucca/) with **Return to official V15**.

1. Get the package: `ncc-0.1.fwsc` (or the newest `ncc-X.Y.fwsc`) from the [Releases](https://github.com/fenderberg/fm1-nc-companion/releases) page, or
   build it yourself (`FELUCCA_LIVEUI=1 ./build.sh`, see [BUILDING.md](../BUILDING.md)).
2. Connect the FM-1 by USB and install with the command-line installer (Python 3 with `pip3 install mido
   python-rtmidi`):

   ```bash
   python3 tools/fm1_install.py ncc-0.1.fwsc
   ```

3. The FM-1 restarts into NCC (identity `FM-1_701` for release 0.1; NCC uses `FM-1_7XY`). Check with `python3 tools/fm1_install.py --info`.

If an install is ever cut off, run the same command again, or hold **OCT-** while switching on to reach the updater.

## Tips

- Put a pad on L with **Drone** on, and an ACID line on R. Ride R's FILTER FREQ and RESONANCE on the Circuit while
  the pad rings.
- Tap **FX** once and you're on SENDS: a quick way to add delay to just one part.
- Hold **ENV** on the last beat before a break for an echo throw.
- On FM6, ALGORITHM flips through patch slots fast. Load your own DX7 bank with Felucca's
  [web editor](https://hugelton.github.io/Felucca/webapp/editor/).
- No Circuit at hand? `DEMO=1 sh tools/ncc_demo.sh` builds the browser emulator with a built-in player.
