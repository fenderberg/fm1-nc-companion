/* SPDX-License-Identifier: GPL-3.0-only */
/* WAVE (NCC, FELUCCA_LIVEUI=1): a wavetable synthesizer. Engine 16, NCC's own.
 *
 * An oscillator that reads a single-cycle wave, and moves through a table of eight of them: POS is the place in the
 * table (between two neighbouring waves, mixed), the track's SHP routings move it (LFO DEST SHP: the scan; ENV DEST
 * SHP, the matrix's SHP) and so does ENV here (the amplitude envelope onto the position, either way: a bright
 * attack that settles, or a slow opening). TABLE picks one of five (tools/gen_ncc_waves.py, worked out there: no
 * recordings): BASIC (sine to saw), VOX (moving formants), GLASS (a phase-modulated sine: bells), PULSE (square to
 * thin), SYNC (a sync sweep). DTN > 0 adds a second oscillator that much apart (the two at half level each), SUB a
 * sine an octave below. Then ANALOG's filter (the trapezoidal SVF's low-pass, CUT / RES, KTR the cutoff with the
 * note) and the track's ADSR.
 *
 * No aliasing to speak of: every wave is stored four times with fewer harmonics (64, 24, 9, 3) and a note reads the
 * level whose top harmonic stays near or below half the sample rate (wave_level: by its phase step, once a block;
 * a glide across a boundary changes level at a block's edge). 8-bit waves, read between samples and between waves.
 * Integers only. State: ph[0] ph[1] the oscillators, ph[2] the sub, s[0] s[1] the filter (kept over a retrigger). */

#define ENGI_WAVE 16u                   /* engines.c ENGINES[] (append-only) */
#include "ncc_waves.h"                  /* tools/gen_ncc_waves.py: NCC_WAVES, NW_* */
static const char *const N_WAVE_TABLE[] = {"BASIC", "VOX", "GLASS", "PULSE", "SYNC", 0};
_Static_assert(NW_TABLES == 5 && NW_WAVES == 8 && NW_LEVELS == 4 && NW_LEN == 256, "WAVE: the tables as the engine reads them");

static void wave_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[1] = v->ph[0] + 0x40000000u;
    v->ph[2] = v->ph[0] >> 1;
    v->s[0] = v->s[1] = 0;                              /* filter */
}

/* the level a phase step reads: 64 harmonics below 330 Hz, 24 below 880, 9 below 2350, else 3 */
static inline uint32_t wave_level(uint32_t inc)
{
    return inc < 32140000u ? 0u : inc < 85700000u ? 1u : inc < 228900000u ? 2u : 3u;
}

/* waves a and b at phase ph, fr (Q15) of the way from a to b: Q15 */
static inline int32_t wave_read(const int8_t *a, const int8_t *b, uint32_t ph, int32_t fr)
{
    uint32_t i = ph >> 24;
    int32_t f = (int32_t)((ph >> 16) & 255u);
    int32_t x0 = a[i] * 256 + (a[i + 1u] - a[i]) * f, x1 = b[i] * 256 + (b[i + 1u] - b[i]) * f;
    return x0 + (((x1 - x0) * fr) >> 15);
}

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, tb = (uint32_t)clamp(p[P_E0], 0, NW_TABLES - 1), inc = m->inc, inc2 = inc, lv = wave_level(inc), u, wi;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t det = p[P_E3], sub = p[P_E6] * 258, fr, ic1 = v->s[0], ic2 = v->s[1];
    int32_t pos = (p[P_E1] << 8) + (m->shape - (64 << 8)) + p[P_E2] * 2 * (m->envq15 >> 7);
    int32_t cut = (p[P_E4] << 8) + m->cutoff + (p[P_E7] * (v->pitch16 - 60 * 16) >> 4);
    const int8_t *a, *b;
    tsvf_t flt;
    u = ((uint32_t)clamp(pos, 0, 127 << 8) * 28898u) >> 11;     /* 0 .. 7 waves on, Q16 (x 7 / 127 / 256 x 65536) */
    if (u > (7u << 16) - 1u)
        u = (7u << 16) - 1u;
    wi = u >> 16;
    fr = (int32_t)((u & 0xFFFFu) >> 1);
    a = NCC_WAVES[tb][wi][lv];
    b = NCC_WAVES[tb][wi + 1u][lv];
    if (det)
        inc2 = cents_inc(m->pitch16, det, m->fine);
    tsvf_coef(&flt, cut, p[P_E5]);
    for (i = 0; i < n; i++) {
        int32_t s = wave_read(a, b, ph0, fr), y;
        ph0 += inc;
        if (det) {
            s = (s + wave_read(a, b, ph1, fr)) >> 1;
            ph1 += inc2;
        }
        if (sub) {
            s += mulq15(osc_sine(ph2), sub);
            ph2 += inc >> 1;
        }
        y = soft_knee(tsvf_lp(&flt, s >> 1, &ic1, &ic2), 16000) << 1;
        out[i] += voice_amp(y, m, i) << 1;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = ic1;
    v->s[1] = ic2;
}

/* {TABLE, POS, ENV, DTN, CUT, RES, SUB, KTR}, {ATK DEC SUS REL}, ENV -> FLT, mono */
static const preset_t WAVE_PRESETS[] = {
    {"WAVE PAD", {0, 40, 30, 12, 80, 10, 0, 48}, {70, 100, 112, 96}, 6, 0, FX(0, 30, 0, 0)},
    {"VOX WAVE", {1, 30, 40, 8, 100, 0, 0, 64}, {50, 100, 110, 90}, 0, 0, FX(0, 20, 0, 0)},
    {"GLASS KEYS", {2, 10, 50, 0, 110, 0, 0, 64}, {0, 90, 30, 60}, 0, 0, FX(0, 15, 0, 0)},
    {"PULSE LEAD", {3, 60, 0, 6, 96, 16, 0, 64}, {2, 70, 104, 36}, 0, 1, FX(0, 0, 0, 0)},
    {"SYNC LEAD", {4, 20, 60, 0, 110, 0, 0, 64}, {2, 96, 80, 36}, 0, 1, FX(0, 0, 0, 0)},
    {"WAVE BASS", {0, 90, 40, 0, 50, 30, 60, 56}, {0, 60, 60, 18}, 30, 1, FX(0, 0, 0, 0)},
    {"DIGI PLUCK", {2, 30, 63, 0, 96, 10, 0, 64}, {0, 56, 0, 40}, 20, 0, FX(0, 0, 0, 0)},
};

static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"WAVE", "FLT"},
    .edit = {
        {"TABLE", F_ENUM, 0, NW_TABLES - 1, 0, N_WAVE_TABLE, 0},
        {"POS", F_PCT, 0, 127, 40, 0, 0},
        {"ENV", F_BIPCT, -64, 63, 0, 0, 0},
        {"DTN", F_INT, 0, 127, 0, 0, "ct"},
        {"CUT", F_CUTOFF, 0, 127, 100, 0, 0},
        {"RES", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"KTR", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = WAVE_PRESETS,
    .npresets = NELEM(WAVE_PRESETS),
    .knob = {P_E1, P_E4, P_ATK, P_REL},
    .keep = 0x03,                       /* the filter */
    .note_on = wave_note_on,
    .render = wave_render,
};
