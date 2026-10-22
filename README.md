# NCC: Novation Circuit Companion

**Firmware that turns the M-VAVE FM-1 into a two-part synth for the Novation Circuit Tracks.**

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

![NCC running in the browser emulator, on a purple FM-1](docs/ncc-emulator.jpg)

## The idea

The Circuit Tracks has two synth tracks inside, plus two **MIDI tracks** and two **audio inputs**. That spare pair is
an open invitation: plug in a second instrument and give each MIDI track its own sound. The FM-1 is small, cheap and
runs [Felucca](https://github.com/hugelton/Felucca), a brilliant multi-engine firmware. But Felucca is a whole
groovebox with a sequencer, songs, menus and a lot to learn.

NCC strips the FM-1 down to one job: **be the Circuit Tracks' companion**.

- **Two parts, two tracks.** Part **L** listens to the Circuit's MIDI 1 track (channel 3), part **R** to MIDI 2
  (channel 4). The Circuit plays the notes and sends the clock. The FM-1 has no sequencer of its own: the Circuit is
  the boss.
- **Two outputs, split by default.** L comes out on the left channel, R on the right. Use a Y-cable into the Circuit's
  two audio inputs and mix, mute and effect each part separately on the Circuit itself.
- **Your Circuit's knobs just work.** For every engine, the Circuit's 8 macro knobs (OSCILLATOR, OSC MOD, AMP ENV,
  FILTER ENV, FILTER FREQ, RESONANCE, MODULATION, FX) are mapped to the parameters that match their labels. Every sound
  is hands-on from the Circuit, with smooth (zipper-free) moves. The FM-1's screen shows what each knob does.
- **Sounds by role, not by engine.** SELECT picks the type (BASS, PAD, LEAD, KEYS, PLUCK, BEAT), ALGORITHM the engine (or
  ALL), PRESETS walks through what's left. A pad is a pad, wherever it comes from.
- **Six engines.** FM6 (6-operator, DX7-style FM), JUNO (analog-style with a Juno chorus and Juno sounds), PHASE
  (Casio CZ-style phase distortion), TRIO (SID-style chip synth), ACID (a 303 with accent and slide) and LOOP: lo-fi
  one-bar drum loops that always fit the bar, opened by any note, with glitch, shuffle, reverse and stutter on the knobs.
- **Built for jamming.** OCT- / OCT+ switch the FM-1's keys and knobs between your two sounds in one press. An arp
  locked to the Circuit's clock, with HOLD. A **Drone** button that keeps a pad ringing while you play over it. Effects
  one tap away (sends, delay, reverb, chorus, insert, and Felucca's PERFORM effects on the keys), plus echo and reverb
  throws.

Less to learn, more to play: NCC gives you more jam with your Circuit Tracks, not more menus.

## Get started

1. **Read the [manual](docs/MANUAL.md).** It's short.
2. **Try it without hardware:** build and open the browser emulator with `sh tools/ncc_demo.sh` (needs Emscripten).
   `DEMO=1 sh tools/ncc_demo.sh` adds a built-in "Circuit" that plays chords and an acid line.
3. **Install it on your FM-1:** see [Installing](docs/MANUAL.md#installing). Back to stock any time with M-VAVE's
   updater or Felucca's web installer (**Return to official V15**).

Installing firmware is at your own risk.

## Status

Early but playable, tested on a real FM-1 with a Circuit Tracks. Feedback and ideas are welcome in the
[issues](https://github.com/fenderberg/fm1-nc-companion/issues).

## Built on Felucca

NCC is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments). All of
Felucca's engines, effects, audio and hardware support are what make NCC possible; NCC adds its own live interface,
the Circuit macro layer, the Juno chorus, the split output and the ACID engine on top. Without the `FELUCCA_LIVEUI=1`
build flag the code builds plain Felucca. Felucca's own README is in [README-Felucca.md](README-Felucca.md); building
is described in [BUILDING.md](BUILDING.md) (NCC: `FELUCCA_LIVEUI=1 ./build.sh`).

Like Felucca, NCC is developed with the help of AI coding agents (Claude Code); the ideas, the sound and the direction
are the author's.

## Licence

Free software: [GPL-3.0-only](LICENSE), as Felucca. The engines in `firmware/engines` are MIT, the bundled fonts and
ported DSP keep their own licences ([LICENSES/](LICENSES/), [LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md)).

Novation and Circuit Tracks are trademarks of Focusrite Audio Engineering Ltd; M-VAVE and FM-1 are trademarks of their
respective owners. NCC is not affiliated with or endorsed by any of them.
