/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC's local demo (web/emu/felucca_web.c with NCC_DEMO=1; tools/ncc_demo.sh builds and serves it): the browser
 * emulator plays as if a Circuit Tracks were plugged in, so NCC is heard and seen without one. Over the emulated USB
 * MIDI (usb.c midi_enqueue, as web_midi's packets arrive) it sends what the Circuit would: the clock (24 a quarter,
 * 120 BPM) and Start, a chord part on channel 3 (L, FM6) and an acid bass line on channel 4 (R, ACID: accents at
 * velocity 120, slides as overlapping notes), and the Circuit's macros moving: R's FILTER FREQ (CC 71) and RESONANCE
 * (CC 74) in slow sweeps, L's MODULATION (CC 12) now and then. Four bars, round and round. The panel works as ever
 * (focus, PRESETS, ARP, FX ..): play along on the keys, turn the knobs.
 * Timing: the device's own millisecond (web_ms), the clock in microseconds (no drift), every note on a clock pulse. */

#define ND_PULSE_US 20833u              /* 120 BPM, 24 pulses a quarter */
#define ND_STEPS 64u                    /* four bars of sixteenths */

/* L: a chord a bar (stabs on steps 0, 6, 10 of each bar, two steps long): Am9 Fmaj7 C6/9 G6 */
static const uint8_t ND_CHORD[4][4] = {{57, 60, 64, 71}, {53, 57, 60, 64}, {48, 55, 57, 62}, {55, 59, 62, 64}};
static const uint8_t ND_STAB[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0};
/* R: the bass line, a note per sixteenth (0 a rest); bit 7 accent, bit 6 slide into the next */
#define A 0x80u
#define S 0x40u
static const uint8_t ND_BASS[ND_STEPS] = {
    33 | A, 33, 45 | S, 33, 0, 33, 36 | A, 33 | S,  45, 33, 0, 43 | A, 33, 33 | S, 40, 0,
    29 | A, 29, 41 | S, 29, 0, 29, 33 | A, 29 | S,  41, 29, 0, 39 | A, 29, 29 | S, 36, 0,
    36 | A, 36, 48 | S, 36, 0, 36, 39 | A, 36 | S,  48, 36, 0, 46 | A, 36, 36 | S, 43, 0,
    31 | A, 31, 43 | S, 31, 0, 31, 34 | A, 31 | S,  43, 31, 0, 41 | A, 43 | S, 45 | S, 47, 0};
#undef A
#undef S

static struct {
    uint8_t on;                         /* set up and running */
    uint32_t us, next;                  /* microseconds since the start, the next pulse */
    uint32_t pulse;                     /* pulses since Start */
    uint8_t bass, chord;                /* the bass note sounding (0 none), the chord's bar + 1 (0 none) */
} nd;

static void nd_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)midi_enqueue((st >= 0xF8u ? 0xFu : st >> 4) | st << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}

static void nd_setup(void)                              /* the parts as the demo wants them, STEREO, the USB clock */
{
    set_engine_of(&trk[0], ENGI_FM6);
    apply_preset_to(&trk[0], 4);                        /* PAD */
    set_engine_of(&trk[1], ENGI_ACID);
    apply_preset_to(&trk[1], 0);                        /* ACID 303 */
    trk[0].p[P_AMODE] = trk[1].p[P_AMODE] = 0;
    song.g[G_CLOCK] = 1;
    song.g[G_ENGGO] = (int16_t)NCC_SPLIT_DEFAULT;       /* STEREO (core.h): headphones hear both */
    nd.on = 1;
    nd.next = ND_PULSE_US * 24u;                        /* a beat of clock before Start */
}

static void nd_step(uint32_t step)                      /* step (0..63) starts */
{
    uint32_t b = ND_BASS[step], prev = ND_BASS[(step + ND_STEPS - 1u) % ND_STEPS], bar = step / 16u, i;
    if (b) {                                            /* the bass: a slide holds the old note into the new one */
        nd_send(0x93, b & 63u, b & 0x80u ? 120u : 90u);
        if (nd.bass && nd.bass != (b & 63u))
            nd_send(0x83, nd.bass, 0);
        nd.bass = (uint8_t)(b & 63u);
    }
    (void)prev;
    if (ND_STAB[step % 16u]) {
        for (i = 0; i < 4u; i++)
            nd_send(0x92, ND_CHORD[bar][i], 84u + i * 6u);
        nd.chord = (uint8_t)(bar + 1u);
    }
    if (step % 16u == 0u)                               /* L's MODULATION: up for bars 2 and 4 */
        nd_send(0xB2, 12, bar & 1u ? 96u : 64u);
}

static void nd_gate(uint32_t step, uint32_t sub)       /* within step: sub pulses (0..5) in */
{
    uint32_t b = ND_BASS[step], i;
    if (sub == 3u && nd.bass && !(b & 0x40u)) {         /* a plain note: half a step; a slide: on to the next */
        nd_send(0x83, nd.bass, 0);
        nd.bass = 0;
    }
    if (sub == 5u && nd.chord && ND_STAB[(step + 15u) % 16u]) {   /* the stab's second step over */
        for (i = 0; i < 4u; i++)
            nd_send(0x82, ND_CHORD[nd.chord - 1u][i], 0);
        nd.chord = 0;
    }
}

/* web_ms, every millisecond once the device runs */
static void ncc_demo_ms(void)
{
    if (!lv.ok)                                         /* (after the live UI's first pass: its defaults, then ours) */
        return;
    if (!nd.on)
        nd_setup();
    nd.us += 1000u;
    while ((int32_t)(nd.us - nd.next) >= 0) {
        nd.next += ND_PULSE_US;
        nd_send(0xF8, 0, 0);
        if (nd.next < ND_PULSE_US * 24u + ND_PULSE_US)
            continue;
        if (!nd.pulse)
            nd_send(0xFA, 0, 0);                        /* Start, with the second beat's first pulse */
        {
            uint32_t step = (nd.pulse / 6u) % ND_STEPS, sub = nd.pulse % 6u;
            if (!sub)
                nd_step(step);
            nd_gate(step, sub);
            if (!sub && step % 2u == 0u) {              /* R's FILTER FREQ and RESONANCE: slow triangles */
                uint32_t ph = (nd.pulse / 12u) % 64u, tri = ph < 32u ? ph * 4u : (63u - ph) * 4u;
                nd_send(0xB3, 71, 24u + tri * 3u / 4u);
                nd_send(0xB3, 74, 60u + tri / 3u);
            }
        }
        nd.pulse++;
    }
}
