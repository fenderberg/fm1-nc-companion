/* SPDX-License-Identifier: GPL-3.0-only */
/* ACID (NCC, FELUCCA_LIVEUI=1): a bass line synthesizer in the manner of the TB-303. Engine 14, NCC's own.
 *
 * One oscillator (WAVE: a band-limited saw, or a square), through a four-pole low-pass ladder (24 dB/oct: four
 * trapezoidal one-poles, the fourth's output fed back over a sample, through a soft clip so a high RES squeals
 * without blowing up), a soft saturation (DRV) after it. A filter envelope of its own, started by each note-on: an
 * instant attack and an exponential DECAY, ENV MOD deep into the cutoff. The VCA is Felucca's ADSR (the presets: a
 * short, punchy one) unless the note is an accent.
 *   ACCENT  a note at velocity 100 or more: more of the envelope into the cutoff, a shorter filter decay and a louder
 *           VCA, by ACC. As on the 303, accents close together build up: each charges the part's accent sweep a
 *           little higher (acid_part_t.sweep, which runs down between them), so a run of accents opens the filter more
 *   SLIDE   the presets play LEGATO with GLIDE in TIME mode: overlapping notes glide to each other and the envelope
 *           runs on (voice.c mono_play: no note-on, so no new filter envelope); a note after a gap retriggers. The
 *           NC's Modulation knob sets the slide's time (perf_layer.c)
 * KTR moves the cutoff with the note (0 none, 127 the octave an octave). Integers only: the ladder in Q15 with Q16
 * coefficients (mulq16), the envelope in Q16, the coefficient's divide once per block (32 bits).
 * State: per voice s[0..3] the ladder (kept over a retrigger: no click), s[4] the filter envelope (Q24), s[5] the
 * note's accent (Q15); per part the accent sweep (engine memory). */

#define ENGI_ACID 14u                   /* engines.c ENGINES[] (append-only) */
static const char *const N_ACID_WAVE[] = {"SAW", "SQR", 0};

typedef struct {
    int32_t sweep;                      /* the accent sweep's charge, Q15 (an accent adds, it runs down) */
} acid_part_t;
ENG_MEM_FITS("ACID", sizeof(acid_part_t));

static acid_part_t *acid_part(track_t *t) { return (acid_part_t *)ENG_MEM((uint32_t)(t - trk) % NPART); }

static void acid_note_on(track_t *t, voice_t *v)
{
    acid_part_t *a = acid_part(t);
    int32_t acc = 0;
    if ((v->mvel ? v->mvel : v->vel) >= 100u) {         /* an accent: the sweep charges a third of the way up */
        a->sweep += (32767 - a->sweep) / 3;
        acc = 16384 + (a->sweep >> 1);                  /* 0.5 .. 1.0 */
    }
    v->s[4] = 1 << 24;                                  /* the filter envelope from its top (Q24) */
    v->s[5] = acc;
}

static void acid_block(track_t *t)                      /* once a block: the accent sweep runs down (~0.5 s) */
{
    acid_part_t *a = acid_part(t);
    a->sweep -= a->sweep >> 9;
}

/* the one-pole coefficient G = g / (1 + g) of cutoff cut (0..127 << 8), Q16 */
static uint32_t acid_g(int32_t cut)
{
    int32_t i, g;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    return ((uint32_t)g << 16) / (4096u + (uint32_t)g);
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, inc = m->inc, ph = v->ph[0], sq = p[P_E0] == 1, G;
    int32_t s0 = v->s[0], s1 = v->s[1], s2 = v->s[2], s3 = v->s[3], env = v->s[4], acc = v->s[5];
    int32_t accamt = (acc * p[P_E5]) >> 7;              /* ACC: how much an accent does, Q15 */
    int32_t envm = p[P_E3] * (32768 + accamt) >> 15;    /* ENV MOD, deeper on an accent: 0 .. ~254 */
    int32_t k = p[P_E2] * 3700 / 127;                   /* RES: feedback 0 .. 0.9 x 4 (Q12) */
    int32_t drv = p[P_E6], vca = 32768 + (accamt >> 1);  /* an accent up to +50 % louder */
    int32_t cut = (p[P_E1] << 8) + m->cutoff + (p[P_E7] * (v->pitch16 - 48 * 16) >> 4) + ((envm * (env >> 8)) >> 8);
    int32_t d = 127 - (acc ? 30 : p[P_E4]);             /* DECAY (an accent: a short one, as on the 303): the fall */
    int32_t dec = 1 + d * d * 40 / 16129;               /* per block, 1/4096: ~70 ms (0), ~250 ms (60), ~3 s (127) */
    G = acid_g(cut);
    for (i = 0; i < n; i++) {
        int32_t x = sq ? osc_pulse(ph, inc, 0x80000000u) : osc_saw(ph, inc), u, y;
        ph += inc;
        u = softclip((x >> 1) - ((k * s3) >> 12));      /* the input less the fed back fourth pole, saturated */
        y = mulq16(u - s0, G);                          /* four trapezoidal one-poles: v = (in - s) G, y = v + s, */
        u = y + s0;                                     /* s = y + v */
        s0 = u + y;
        y = mulq16(u - s1, G);
        u = y + s1;
        s1 = u + y;
        y = mulq16(u - s2, G);
        u = y + s2;
        s2 = u + y;
        y = mulq16(u - s3, G);
        u = y + s3;
        s3 = clamp(u + y, -98304, 98304);
        u = clamp(u * (4096 + k / 3) >> 12, -131072, 131071);   /* (the passband's loss at high RES, part made up) */
        if (drv)                                        /* (pre-shifts: 1x .. 2.5x, no overflow) */
            u = softclip(((u >> 2) * ((32768 + drv * 384) >> 2)) >> 11) << 1;
        out[i] += voice_amp((soft_knee(u, 30000) * (vca >> 1)) >> 14, m, i) << 1;
    }
    env -= (env >> 12) * dec;                           /* (once a block; Q24: no step left stuck near 0) */
    v->ph[0] = ph;
    v->s[0] = s0;
    v->s[1] = s1;
    v->s[2] = s2;
    v->s[3] = s3;
    v->s[4] = env;
}

/* {WAVE, CUT, RES, ENV, DECY, ACC, DRV, KTR}, {ATK DEC SUS REL}, ENV -> FLT, mono */
static const preset_t ACID_PRESETS[] = {
    {"ACID 303", {0, 34, 96, 80, 60, 90, 20, 32}, {0, 64, 100, 18}, 0, 1, FX(0, 0, 25, 10)},
    {"SQUELCH", {0, 22, 118, 110, 45, 110, 40, 32}, {0, 56, 90, 16}, 0, 1, FX(10, 0, 30, 10)},
    {"SQR ACID", {1, 30, 100, 90, 55, 100, 25, 32}, {0, 60, 100, 18}, 0, 1, FX(0, 0, 25, 10)},
    {"DEEP BASS", {0, 18, 60, 40, 80, 60, 10, 16}, {0, 70, 110, 24}, 0, 1, FX(0, 0, 0, 5)},
    {"RUBBER", {1, 26, 80, 70, 30, 80, 30, 32}, {0, 40, 60, 14}, 0, 1, FX(0, 0, 15, 10)},
    {"SCREAM", {0, 40, 124, 120, 70, 127, 90, 48}, {0, 64, 100, 20}, 0, 1, FX(30, 0, 35, 15)},
    {"DUB ACID", {0, 28, 104, 85, 90, 90, 15, 32}, {0, 72, 100, 30}, 0, 1, FX(0, 0, 60, 30)},
    {"HOOVERISH", {1, 50, 90, 60, 100, 70, 60, 64}, {4, 80, 110, 40}, 0, 1, FX(20, 0, 30, 25)},
};

static const engine_t ENG_ACID = {
    .name = "ACID",
    .page_title = {"VCO VCF", "ENV"},
    .edit = {
        {"WAVE", F_ENUM, 0, 1, 0, N_ACID_WAVE, 0},
        {"CUT", F_CUTOFF, 0, 127, 34, 0, 0},
        {"RES", F_PCT, 0, 127, 96, 0, 0},
        {"ENV", F_PCT, 0, 127, 80, 0, 0},
        {"DECY", F_PCT, 0, 127, 60, 0, 0},
        {"ACC", F_PCT, 0, 127, 90, 0, 0},
        {"DRV", F_PCT, 0, 127, 20, 0, 0},
        {"KTR", F_PCT, 0, 127, 32, 0, 0},
    },
    .presets = ACID_PRESETS,
    .npresets = NELEM(ACID_PRESETS),
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = 1,
    .keep = 0x0Fu,                      /* the ladder over a retrigger: no click */
    .note_on = acid_note_on,
    .render = acid_render,
    .block = acid_block,
    .mem = sizeof(acid_part_t),
};
