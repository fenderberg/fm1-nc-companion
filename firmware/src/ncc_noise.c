/* SPDX-License-Identifier: GPL-3.0-only */
/* NC: NOISE's existing pink/brown poles can exceed 16 bits (e.g. LFSR COLR=56). Their products and the
 * SVF accumulators need wide multiplies, not wide divisions. Keep upstream's presets and descriptor intact.
 * The renderer below is derived from eng_noise.c (MIT, Leo Kuroshita, Hügelton Instruments, 2026). */
#define noise_render ncc_noise_original_render
#define ENG_NOISE NCC_NOISE_ORIGINAL
#include "../engines/src/eng_noise.c"
#undef ENG_NOISE
#undef noise_render
static void noise_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t mode = (uint32_t)p[P_E0] & 3u, sel = ((uint32_t)p[P_E5] >> 4) & 7u, i;
    int32_t res = p[P_E3], drft = p[P_E6], crsh = mode < NZ_LFSR ? p[P_E7] : 0;
    int32_t ic1 = v->s[0], ic2 = v->s[1], rng = v->s[2], b0 = v->s[4], b1 = v->s[5], b2 = v->s[6], br = v->s[7];
    uint32_t ph = v->ph[0], reg = v->ph[1];         /* state in locals: out[] may alias v->s[] */
    int32_t drift = (int32_t)v->ph[2], d16 = 0, gain = 32767, gq, colr, cut, k, a1, a2, a3, g, bpg, mix, cf, thr = 0;
    uint32_t inc = 0, taps = NOISE_TAPS[sel];
    /* drift: a new random target every 0.7..3 s, followed with a 0.7 s lag */
    if (drft) {
        int32_t tg = v->s[3] >> 16, cnt = v->s[3] & 0xFFFF;
        if (--cnt <= 0) {
            uint32_t r = noise32(&rng);
            tg = (int32_t)(r >> 16) - 32768;
            cnt = 1024 + (int32_t)(r & 3071u);
        }
        v->s[3] = (int32_t)((uint32_t)tg << 16 | (uint32_t)cnt);
        drift += ((tg << 4) - drift) >> 10;           /* Q19 */
        d16 = ((drift >> 11) * drft * drft) >> 14;    /* up to about +-16 semitones */
        gain = 32767 - ((((32767 - (drift >> 4)) >> 1) * drft) >> 8);   /* down to -6 dB as it falls */
    }
    /* COLR (+ SHP), 0..127 << 8: white -> pink -> brown */
    colr = clamp((p[P_E1] << 8) + m->shape - (64 << 8), 0, 127 << 8);
    cf = colr <= (64 << 8) ? colr << 1 : ((colr - (64 << 8)) * 520) >> 8;   /* the crossfade, Q15 */
    /* FREQ (+ FLT), key-tracked by TRK around C4, the drift: SVF cutoff units (256 an index, 0.86 st) */
    cut = (p[P_E2] << 8) + m->cutoff + (((m->pitch16 - 60 * 16) * p[P_E4] * 151) >> 10) + ((d16 * 4785) >> 8);
    cut = clamp(cut, 0, 127 << 8);
    g = SVF_G[cut >> 8];
    if (cut < (127 << 8))
        g += ((SVF_G[(cut >> 8) + 1] - g) * (cut & 255)) >> 8;
    k = 8192 - res * 7800 / 127;                      /* damping, Q12: 2.0 .. 0.1 */
    a1 = (int32_t)((4096u << 13) / (uint32_t)(4096 + ((g * (g + k)) >> 12)));
    a2 = (a1 * g) >> 12;
    a3 = (a2 * g) >> 12;
    bpg = (int32_t)isqrt32((uint32_t)k << 12);        /* band-pass x 1.5 sqrt(damping): its level holds with the Q */
    mix = res * 258;                                  /* low-pass -> band-pass, Q15 */
    {   /* level: make up most of what the filter takes from white noise (less for pink and brown), +12 dB for
         * the sparse DUST, the drift's level; Q12, mk in 1/256 octave: 2^(mk / 256), at most x 16 */
        int32_t wf = 32767 - ((colr * 230) >> 8), mk = (((((127 << 8) - cut) >> 8) * 7) * wf) >> 15;
        mk += 214 + (mode == NZ_DUST ? 512 : 0);
        if (mk > 1279)
            mk = 1279;
        gq = (4096 + ((mk & 255) << 4)) << (mk >> 8);
        gq = (gq * (gain >> 3)) >> 12;
        if (gq > 65535)
            gq = 65535;
    }
    if (mode <= NZ_DUST && p[P_E5]) {                 /* impulses: 1 .. 2000 a second */
        uint32_t e = (uint32_t)p[P_E5] * 22u, r = 1u << (e >> 8);   /* 2^(DENS * 11 / 127), Q8 fraction */
        r += (r * (e & 255u)) >> 8;
        thr = (int32_t)((r * 97391u) >> 16);          /* per 2^16 of a sample: rate * 65536 / FS, Q16 */
    }
    if (mode == NZ_LFSR) {
        uint32_t mask = taps;                         /* the register's own bits (MODE / LEN changes) */
        inc = noise_inc(m->pitch16 + p[P_E7] * 16 + d16);
        mask |= mask >> 1;
        mask |= mask >> 2;
        mask |= mask >> 4;
        mask |= mask >> 8;
        mask |= mask >> 16;
        if (!(reg & mask))                            /* a new note: the register from the note's seed */
            reg = (uint32_t)rng | 1u;
        reg &= mask;
    } else if (mode == NZ_META) {
        inc = p[P_E7] || d16 ? noise_inc(m->pitch16 + p[P_E7] * 16 + d16) : m->inc;
    }
    for (i = 0; i < n; i++) {
        int32_t w, x, v1, v2, v3, y;
        switch (mode) {
        case NZ_DUST:
        case NZ_ANLG: {
            uint32_t r = noise32(&rng);
            w = mode == NZ_ANLG ? (int32_t)(r >> 17) - 16384 : 0;
            if ((int32_t)(r & 0xFFFFu) < thr) {       /* an impulse, random size and sign */
                int32_t a = 8192 + (int32_t)((r >> 18) & 0x7FFFu);
                w += (r & 0x20000u) ? a : -a;
            }
            break;
        }
        case NZ_LFSR: {
            uint32_t o = ph;
            ph += inc;
            if (ph < o)
                reg = (reg >> 1) ^ (-(reg & 1u) & taps);
            w = (reg & 1u) ? 12000 : -12000;
            break;
        }
        default: {
            uint32_t j = ((ph >> 16) * NOISE_SEQ[sel].len) >> 16;
            ph += inc;
            w = (NOISE_SEQ[sel].w[j >> 5] >> (j & 31u)) & 1u ? NOISE_SEQ[sel].hi : NOISE_SEQ[sel].lo;
            break;
        }
        }
        x = w;
        if (colr) {                                   /* pink: three leaky poles (Kellet), brown: a leaky integrator */
            int32_t pk;
            b0 = (((int64_t)b0 * 32690) >> 15) + ((w * 3245) >> 15);
            b1 = (((int64_t)b1 * 31555) >> 15) + ((w * 9716) >> 15);
            b2 = (((int64_t)b2 * 18678) >> 15) + ((w * 17247) >> 14);
            pk = (b0 + b1 + b2 + ((w * 6055) >> 15)) >> 2;
            if (colr <= (64 << 8)) {
                x = w + (int32_t)(((int64_t)(pk - w) * cf) >> 15);
            } else {
                br = (((int64_t)br * 32580) >> 15) + ((w * 3480) >> 15);
                x = pk + (int32_t)(((int64_t)(br - pk) * cf) >> 15);
            }
        }
        /* the SVF (dsp.c tsvf_lp, with the band-pass), rounded: at low cutoffs the floor of the shifts
         * walked the integrators off zero (DC) */
        v3 = x - ic2;
        v1 = ((int64_t)a1 * ic1 + (int64_t)a2 * v3 + 4096) >> 13;
        v2 = ic2 + (((int64_t)a2 * ic1 + (int64_t)a3 * v3 + 4096) >> 13);
        ic1 = clamp(2 * v1 - ic1, -150000, 150000);
        ic2 = clamp(2 * v2 - ic2, -150000, 150000);
        v1 = (int32_t)(((int64_t)v1 * bpg * 3) >> 13);
        y = v2 + (int32_t)(((int64_t)(v1 - v2) * (mix >> 3)) >> 12);
        y = soft_knee((int32_t)(((int64_t)(y >> 4) * gq) >> 8), 24000);
        if (mode == NZ_META) {                        /* the knee bends a lopsided tone: a DC blocker, ~14 Hz */
            reg += (uint32_t)(((y << 8) - (int32_t)reg) >> 9);
            y -= (int32_t)reg >> 8;
        }
        if (crsh) {                                   /* hold 1..16 samples, 16 .. 4 bits */
            if ((int32_t)--ph <= 0 || ph > 16u) {
                ph = 1u + (uint32_t)crsh / 8u;
                reg = (uint32_t)(y >> (crsh / 11) << (crsh / 11));
            }
            y = (int32_t)reg;
        }
        out[i] += voice_amp(y, m, i);
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = rng;
    v->s[4] = b0;
    v->s[5] = b1;
    v->s[6] = b2;
    v->s[7] = br;
    v->ph[0] = ph;
    v->ph[1] = reg;
    v->ph[2] = (uint32_t)drift;
}

/* {MODE, COLR, FREQ, RES, TRK, DENS, DRFT, CRSH / CLK} */
/* NC: same descriptor and presets as upstream; only render arithmetic differs. */
static const engine_t ENG_NOISE = {
    .name = "NOISE",
    .page_title = {"SRC", "MOVE"},
    .edit = {
        {"MODE", F_ENUM, 0, 3, 0, N_NOISE_MODE, 0},
        {"COLR", F_PCT, 0, 127, 64, 0, 0},
        {"FREQ", F_CUTOFF, 0, 127, 80, 0, 0},
        {"RES", F_PCT, 0, 127, 30, 0, 0},
        {"TRK", F_PCT, 0, 127, 64, 0, 0},
        {"DENS", F_PCT, 0, 127, 0, 0, 0},           /* LFSR, META: the register (noise_desc) */
        {"DRFT", F_PCT, 0, 127, 0, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},           /* LFSR, META: the clock above the key */
    },
    .presets = NOISE_PRESETS,
    .npresets = NELEM(NOISE_PRESETS),
    .note_on = noise_note_on,
    .render = noise_render,
    .knob = {P_E2, P_E3, P_E1, P_REL},
    .keep = 0xF3,                /* the filter, the colour poles */
    .desc = noise_desc,
};
