/* SPDX-License-Identifier: GPL-3.0-only */
/* LOOP (NCC, FELUCCA_LIVEUI=1): a drum loop player for the Circuit Tracks' MIDI tracks. Engine 15, NCC's own.
 *
 * A loop is one bar (tools/gen_loops.py: built from Felucca's own drums, lo-fi on purpose: 11025 Hz, 8 bits, so any
 * point of it can be read at once). It always fits the bar: it is played as 16 slices, each started on its 16th of
 * the clock (ncc_seq.c ncc_bar: samples into the bar, the Circuit's clock or the BPM), at its own speed (PTCH aside):
 * a faster tempo cuts a slice short, a slower one leaves a little silence after it. No time stretch, no pitch change.
 *   THE NOTE IS A GATE  any note, any pitch (the Circuit's scale and key do not matter), opens the loop where the bar
 *       is now; its note-off closes it (the ADSR's short release). The Circuit's steps become the loop's rhythm; a
 *       long note plays it all; HOLD / Drone keep it open. Velocity: the level (voice.c)
 *   PTCH  the slices' speed, -12 .. +12 semitones (still one per 16th)
 *   SHUF  each slice's chance to be another one of the bar (beat shuffle)
 *   GATE  how much of each slice plays (127 all of it, down to blips)
 *   REV   each slice's chance to play backwards
 *   FILT  a DJ filter: left of 0 a low-pass closing, right a high-pass opening (0: none)
 *   CRSH  bit and rate crush (16 down to 3 bits, a held sample for 1 .. 24 samples)
 *   STUT  1 .. 63: each 16th's chance of a ratchet (2, 3 or 4 hits of its slice); 64 .. 127: beat repeat, the slice
 *         caught when it began repeats every 1/8, 1/16, 1/32, 1/64 (the higher, the faster)
 * One voice (LEGATO: overlapping notes keep it open). Integers only: the position in Q16 source samples, the speed a
 * table, the filter Simper's SVF with Q16 coefficients (one divide a block, 32 bits), 64-bit products only.
 * State: per voice ph[0] the read position, ph[1] the slice's end (backwards: its start), s[0] the 16th / sub-hit,
 * s[1] the gate's samples left, s[2] backwards, s[3] the fade (Q15), s[4] / s[5] the filter, s[6] the caught slice
 * + 1 (beat repeat), s[7] the hits in this 16th; per part the random state and the crush's held sample. */

#include "ncc_loops.h"                  /* (build/gen: tools/gen_loops.py) */

#define ENGI_LOOP 15u                   /* engines.c ENGINES[] (append-only) */
#define LP_SLICES 16u
#define LP_FADE 48                      /* samples of a slice's fade in / out (~1 ms) */

static uint32_t ncc_bar;                /* (ncc_seq.c: samples into the bar at this block's start) */
static uint32_t beat_samples(void);     /* (fx.c) */

typedef struct {
    uint32_t rng;                       /* xorshift */
    int32_t held;                       /* CRSH: the held sample and its count */
    uint32_t hold_n;
} loop_part_t;
ENG_MEM_FITS("LOOP", sizeof(loop_part_t));

static loop_part_t *loop_part(track_t *t) { return (loop_part_t *)ENG_MEM((uint32_t)(t - trk) % NPART); }

static uint32_t loop_rand(loop_part_t *a)               /* 0 .. 127 */
{
    uint32_t x = a->rng ? a->rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    a->rng = x;
    return (x >> 9) & 127u;
}

/* 2^(s / 12) in Q16, s = -12 .. 12 */
static const uint32_t LP_SEMI[25] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386,
                                     61858, 65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218,
                                     116772, 123715, 131072};

static void loop_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = -1;                                       /* (the first sample opens the loop where the bar is) */
    v->s[3] = 0;
    v->s[6] = 0;
}

/* the 16th k (sub-hit j) begins, at off samples into it (a note-on mid-slice: the slice from there) */
static void loop_hit(track_t *t, voice_t *v, uint32_t k, uint32_t j, uint32_t off, uint32_t unit, uint32_t inc)
{
    const int16_t *p = t->p;
    loop_part_t *a = loop_part(t);
    uint32_t lp = (uint32_t)p[P_E0] % NLOOP, len = NLOOP_OFF[lp + 1] - NLOOP_OFF[lp], sl = len / LP_SLICES;
    uint32_t src = k, stut = (uint32_t)clamp(p[P_E7], 0, 127), rev, span = sl;
    if (stut >= 64u) {                                  /* beat repeat: the slice caught when the repeat began */
        if (!v->s[6])
            v->s[6] = (int32_t)k + 1;
        src = (uint32_t)v->s[6] - 1u;
        if (stut < 80u)                                 /* 1/8: two 16ths of it from the caught one */
            span = 2u * sl, src &= ~1u;
    } else {
        v->s[6] = 0;
        if (!j && loop_rand(a) < (uint32_t)clamp(p[P_E2], 0, 127))
            src = loop_rand(a) & (LP_SLICES - 1u);      /* SHUF: another slice of the bar */
    }
    rev = !j ? loop_rand(a) < (uint32_t)clamp(p[P_E4], 0, 127) : (uint32_t)v->s[2];
    v->s[2] = (int32_t)rev;
    if (rev) {                                          /* backwards: from the slice's end down to its start */
        v->ph[1] = (src * sl) << 16;
        v->ph[0] = ((src * sl + span - 1u) << 16) - inc * off;
    } else {
        v->ph[1] = (src * sl + span) << 16;
        v->ph[0] = ((src * sl) << 16) + inc * off;
    }
    v->s[1] = (int32_t)((unit * (uint32_t)clamp(p[P_E3], 1, 127)) >> 7);   /* GATE */
    v->s[1] = v->s[1] > (int32_t)off ? v->s[1] - (int32_t)off : 0;
    v->s[3] = off ? 32767 : 0;                          /* (mid-slice: open at once; a hit: a short fade in) */
}

static void loop_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    loop_part_t *a = loop_part(t);
    uint32_t lp = (uint32_t)p[P_E0] % NLOOP, base = NLOOP_OFF[lp], len = NLOOP_OFF[lp + 1] - base;
    uint32_t bar = 4u * beat_samples(), i, stut = (uint32_t)clamp(p[P_E7], 0, 127), unit, sub, grp, k, pos, inc;
    uint32_t crush = (uint32_t)clamp(p[P_E6], 0, 127), hold = 1u + crush * 23u / 127u, bits = 16u - crush * 13u / 127u;
    int32_t f = clamp(p[P_E5], -64, 63), ic1 = v->s[4], ic2 = v->s[5], a1 = 0, a2 = 0, a3 = 0, kq = 5000, g;
    const int8_t *d = NLOOP_DATA + base;
    if (bar < 4u * LP_SLICES * 16u)
        bar = 4u * LP_SLICES * 16u;
    unit = bar / LP_SLICES;
    inc = (((uint32_t)NLOOP_RATE << 16) / FS * LP_SEMI[clamp(p[P_E1], -12, 12) + 12]) >> 16;   /* (<= 2^31: 32 bits) */
    if (f) {                                            /* FILT: the SVF's coefficients (Simper), a divide a block */
        g = SVF_G[f < 0 ? clamp(127 + 2 * f, 0, 127) : clamp(2 * f, 0, 127)];
        a1 = (int32_t)((1u << 28) / (4096u + (uint32_t)((g * (g + kq)) >> 12)));
        a2 = (g * a1) >> 12;
        a3 = (g * a2) >> 12;
    }
    sub = stut >= 112u ? 4u : stut >= 96u ? 2u : 1u;    /* the hits in a 16th (beat repeat 1/32, 1/64) */
    grp = stut >= 64u && stut < 80u ? 2u : 1u;          /* (beat repeat 1/8: a hit every other 16th) */
    pos = ncc_bar % bar;
    for (i = 0; i < n; i++, pos = pos + 1u < bar ? pos + 1u : 0u) {
        uint32_t kk = pos / unit, nh = sub, j, hl, in;
        int32_t x = 0;
        k = kk & (LP_SLICES - 1u);
        kk -= k % grp;
        k -= k % grp;
        in = pos - kk * unit;
        if (stut && stut < 64u && (int32_t)k != (v->s[0] >> 4))   /* a new 16th: a ratchet? */
            nh = loop_rand(a) < stut * 2u ? 2u + loop_rand(a) % 3u : 1u;
        else if (stut && stut < 64u)
            nh = (uint32_t)v->s[7];
        hl = unit * grp / (nh ? nh : 1u);
        j = in / (hl ? hl : 1u);
        if (v->s[0] < 0 || (int32_t)(k << 4 | j) != v->s[0]) {   /* a hit begins (or the note opened mid-way) */
            uint32_t off = v->s[0] < 0 ? in - j * hl : 0u;
            v->s[7] = (int32_t)nh;
            v->s[0] = (int32_t)(k << 4 | j);
            loop_hit(t, v, k, j, off, hl, inc);
        }
        if (v->s[1] > 0 && (v->s[2] ? v->ph[0] >= v->ph[1] && v->ph[0] < len << 16 : v->ph[0] < v->ph[1])) {
            uint32_t ix = v->ph[0] >> 16, fr = v->ph[0] & 0xFFFFu, ix2 = ix + 1u < len ? ix + 1u : 0u;
            x = ((int32_t)d[ix] * (int32_t)(65536u - fr) + (int32_t)d[ix2] * (int32_t)fr) >> 8;   /* 16 bits */
            v->ph[0] = v->s[2] ? v->ph[0] - inc : v->ph[0] + inc;
            v->s[1]--;
            v->s[3] = v->s[1] < LP_FADE ? (v->s[1] * 32767) / LP_FADE : clamp(v->s[3] + 32767 / LP_FADE, 0, 32767);
        } else
            v->s[3] = 0;
        x = (x * v->s[3]) >> 15;
        if (f) {                                        /* the SVF: v3 = x - ic2, v1 = a1 ic1 + a2 v3, v2 = ic2 + .. */
            int32_t v3 = x - ic2, v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 16);
            int32_t v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 16);
            ic1 = clamp(2 * v1 - ic1, -262144, 262143);
            ic2 = clamp(2 * v2 - ic2, -262144, 262143);
            x = f < 0 ? v2 : x - ((kq * v1) >> 12) - v2;
        }
        if (crush) {                                    /* CRSH: a held sample, fewer bits */
            if (++a->hold_n >= hold) {
                a->hold_n = 0;
                a->held = (clamp(x, -32768, 32767) >> (16u - bits)) * (1 << (16u - bits));
            }
            x = a->held;
        }
        out[i] += voice_amp(clamp(x, -32767, 32767), m, i) << 1;
    }
    v->s[4] = ic1;
    v->s[5] = ic2;
}

/* {LOOP, PTCH, SHUF, GATE, REV, FILT, CRSH, STUT}, {ATK DEC SUS REL}: open at once, a short release */
static const preset_t LOOP_PRESETS[] = {
    {"JUNGLE BREAK", {0, 0, 0, 127, 0, 0, 0, 0}, {0, 0, 127, 6}, 0, 1, FX(0, 0, 0, 10)},
    {"BOOM BAP BEAT", {1, 0, 0, 127, 0, -10, 20, 0}, {0, 0, 127, 6}, 0, 1, FX(10, 0, 0, 10)},
    {"TWO STEP BEAT", {2, 0, 0, 127, 0, 0, 0, 0}, {0, 0, 127, 6}, 0, 1, FX(0, 0, 10, 10)},
    {"FOUR FLOOR BEAT", {3, 0, 0, 127, 0, 0, 0, 0}, {0, 0, 127, 6}, 0, 1, FX(0, 0, 0, 5)},
    {"ELECTRO BEAT", {4, 0, 0, 110, 0, 0, 10, 0}, {0, 0, 127, 6}, 0, 1, FX(0, 0, 10, 5)},
    {"IDM GLITCH BEAT", {5, 0, 20, 100, 10, 0, 30, 20}, {0, 0, 127, 6}, 0, 1, FX(10, 0, 20, 15)},
    {"AMEN-ISH BREAK", {6, 0, 0, 127, 0, 0, 0, 0}, {0, 0, 127, 6}, 0, 1, FX(0, 0, 0, 10)},
};
_Static_assert(NELEM(LOOP_PRESETS) == NLOOP, "a preset per loop (tools/gen_loops.py)");

static const engine_t ENG_LOOP = {
    .name = "LOOP",
    .page_title = {"LOOP", "FX"},
    .edit = {
        {"LOOP", F_ENUM, 0, NLOOP - 1, 0, NLOOP_NAME, 0},
        {"PTCH", F_INT, -12, 12, 0, 0, "st"},
        {"SHUF", F_PCT, 0, 127, 0, 0, 0},
        {"GATE", F_PCT, 0, 127, 127, 0, 0},
        {"REV", F_PCT, 0, 127, 0, 0, 0},
        {"FILT", F_BIPCT, -64, 63, 0, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},
        {"STUT", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = LOOP_PRESETS,
    .npresets = NELEM(LOOP_PRESETS),
    .knob = {P_E2, P_E4, P_E5, P_E7},
    .poly = 1,
    .sampled = 1,
    .keep = 0x30u,                      /* the filter over a retrigger: no click */
    .note_on = loop_note_on,
    .render = loop_render,
    .mem = sizeof(loop_part_t),
};
