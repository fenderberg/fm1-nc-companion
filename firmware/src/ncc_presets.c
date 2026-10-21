/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): NCC's own sounds (main loop; the library lists each engine's before its presets,
 * ncc_library.c), in the engines' own terms, with no change to the engines (Felucca's, MIT, kept as they are).
 *   JUNO (ANALOG)  a Juno's voice: WAVE SUB (OSC 1 a saw, OSC 2 a square an octave below it, MIX its level: the DCO and
 *                  its sub) or PWM (the LFO on SHP sweeps the width), its SVF low-pass and ADSR; the Juno chorus and
 *                  the drift are P_ED_FX (juno_chorus.c: chorus | drift << 2). Every ANALOG sound here has some drift,
 *                  so P_ED_FX >= 4 marks one after a power-on (the column then says JUNO: which one is not stored)
 *   PHASE          the keys, plucks, basses and pads of a phase distortion synth: DCW the brightness, ENV its sweep
 *   TRIO           chip sounds: three oscillators (WAVE the set), INT2 / INT3 their intervals, the multimode filter
 *   ACID           more 303: a lead, a pluck, a soft bass (one voice, LEGATO: they slide)
 * The names say what a sound is for (ncc_library.c nl_cat reads them: BASS PAD LEAD KEYS PLUCK; tests/ncc_test.c: none
 * of them OTHER). Loaded as one sound load: the engine's preset 0 for the defaults of everything, then these values,
 * inside one load_begin / load_end (one undo, the fades of a load). */

typedef struct {
    uint8_t eng;                        /* ENGINES[] index */
    const char *name;
    int8_t e[8];                        /* P_E0..P_E7 (ANALOG: WAVE DTN MIX NOIS CUT RES DRV KTR) */
    uint8_t env[4];                     /* ATK DEC SUS REL */
    int8_t fenv;                        /* ENV -> FILTER (PHASE: its own ENV -> DCW is E3) */
    uint8_t mono;                       /* 1: LEGATO, 0: POLY */
    uint8_t fx[4];                      /* DIST, CHORUS (the bus), DELAY, REVERB sends */
    uint8_t lrate;                      /* LFO RATE */
    int8_t lshp, lflt;                  /* LFO -> SHP (PWM's width, TRIO's PW), LFO -> FLT */
    uint8_t jx;                         /* P_ED_FX: Juno chorus (0 OFF, 1 I, 2 II, 3 I+II) | drift (0..15) << 2 */
} ncc_snd_t;

#define JX(chorus, drift) (uint8_t)((chorus) | (drift) << 2)
#define NJ_SAW 0
#define NJ_SQR 1
#define NJ_TRI 2
#define NJ_PWM 4
#define NJ_SYNC 5
#define NJ_SUB 6                       /* (eng_analog.c AW_SUB) */
#define J ENGI_ANALOG
#define PH ENGI_PHASE
#define TR ENGI_TRIO
#define AC ENGI_ACID
static const ncc_snd_t NCC_SND[] = {
    /* JUNO: {WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR}, {A D S R}, fenv, mono, {DST CHO DLY REV}, LFO rate, shp, flt */
    {J, "JUNO STRINGS", {NJ_PWM, 0, 0, 0, 72, 6, 0, 48}, {55, 90, 112, 80}, 4, 0, {0, 0, 10, 45}, 34, 26, 0, JX(2, 6)},
    {J, "JUNO PAD", {NJ_SUB, 4, 40, 4, 56, 14, 0, 40}, {92, 100, 116, 100}, 10, 0, {0, 0, 20, 65}, 18, 0, 6, JX(1, 8)},
    {J, "JUNO BRASS", {NJ_SAW, 0, 0, 0, 40, 16, 0, 64}, {26, 70, 96, 40}, 34, 0, {0, 0, 10, 30}, 40, 0, 0, JX(1, 4)},
    {J, "POLY SYNTH", {NJ_SUB, 0, 30, 0, 60, 24, 0, 64}, {2, 76, 70, 50}, 24, 0, {0, 0, 25, 35}, 40, 0, 0, JX(1, 5)},
    {J, "JUNO ORGAN", {NJ_SQR, 0, 0, 0, 92, 0, 0, 64}, {0, 10, 127, 14}, 0, 0, {0, 0, 0, 30}, 40, 0, 0, JX(3, 2)},
    {J, "JUNO BASS", {NJ_SUB, 0, 70, 0, 34, 36, 10, 64}, {0, 60, 40, 20}, 30, 1, {8, 0, 0, 10}, 40, 0, 0, JX(0, 1)},
    {J, "SUB BASS", {NJ_SUB, 0, 127, 0, 30, 0, 0, 32}, {0, 50, 110, 16}, 6, 1, {0, 0, 0, 5}, 40, 0, 0, JX(0, 1)},
    {J, "JUNO PLUCK", {NJ_SAW, 0, 0, 0, 30, 30, 0, 64}, {0, 56, 0, 46}, 44, 0, {0, 0, 30, 25}, 40, 0, 0, JX(2, 4)},
    {J, "JUNO LEAD", {NJ_SUB, 0, 20, 0, 70, 30, 0, 64}, {2, 70, 100, 36}, 14, 1, {0, 0, 35, 25}, 40, 0, 0, JX(1, 3)},
    {J, "HOOVER", {NJ_PWM, 0, 0, 0, 80, 10, 30, 64}, {6, 80, 110, 50}, 8, 1, {20, 0, 15, 30}, 70, 50, 0, JX(3, 10)},
    {J, "JUNO BELLS", {NJ_SQR, 0, 0, 0, 100, 40, 0, 96}, {0, 70, 0, 80}, 10, 0, {0, 0, 25, 60}, 40, 0, 0, JX(2, 3)},
    {J, "JUNO SWEEP", {NJ_SAW, 0, 0, 0, 20, 70, 0, 64}, {80, 110, 100, 90}, 50, 0, {0, 0, 20, 60}, 14, 0, 26, JX(1, 6)},
    /* .. NCC 0.2: the keys and plucks it lacked, a pad, a bass, a lead */
    {J, "JUNO EP", {NJ_SUB, 0, 24, 0, 50, 8, 0, 80}, {0, 86, 44, 58}, 22, 0, {0, 0, 15, 35}, 40, 0, 0, JX(2, 4)},
    {J, "FUNK CLAV", {NJ_PWM, 0, 0, 0, 58, 44, 10, 96}, {0, 48, 0, 26}, 40, 0, {0, 0, 10, 15}, 40, 14, 0, JX(0, 2)},
    {J, "HOUSE KEYS", {NJ_SAW, 6, 64, 0, 62, 12, 0, 64}, {0, 82, 36, 46}, 30, 0, {0, 0, 25, 35}, 40, 0, 0, JX(1, 4)},
    {J, "GLASS PLUCK", {NJ_SYNC, 40, 0, 0, 70, 20, 0, 80}, {0, 60, 0, 50}, 24, 0, {0, 0, 35, 40}, 40, 0, 0, JX(2, 4)},
    {J, "ARP PLUCK", {NJ_SUB, 0, 50, 0, 36, 40, 0, 64}, {0, 44, 0, 30}, 46, 0, {0, 0, 30, 20}, 40, 0, 0, JX(1, 3)},
    {J, "DREAM PAD", {NJ_PWM, 0, 0, 6, 48, 20, 0, 48}, {100, 110, 112, 110}, 16, 0, {0, 0, 30, 80}, 22, 30, 10, JX(3, 9)},
    {J, "SHIMMER PAD", {NJ_SAW, 14, 64, 0, 84, 10, 0, 64}, {84, 100, 116, 108}, 6, 0, {0, 0, 40, 90}, 30, 0, 4, JX(2, 7)},
    {J, "OCTAVE BASS", {NJ_SUB, 0, 96, 0, 44, 30, 16, 64}, {0, 58, 70, 18}, 28, 1, {6, 0, 0, 5}, 40, 0, 0, JX(0, 1)},
    {J, "SQUARE LEAD", {NJ_SQR, 6, 0, 0, 74, 20, 10, 64}, {2, 70, 104, 40}, 12, 1, {0, 0, 40, 25}, 40, 0, 0, JX(1, 3)},
    /* PHASE: {WAVE, WAVE2, DCW, ENV, DTN, LINE, SUB, FOLD} (WAVE SAW SQR PLS DSIN SPLS RSAW RTRI RTRP) */
    {PH, "PD BASS", {0, 0, 20, 100, 0, 0, 64, 0}, {0, 60, 40, 20}, 0, 1, {6, 0, 0, 8}, 40, 0, 0, 0},
    {PH, "PD EP", {3, 0, 24, 64, 6, 0, 0, 0}, {0, 92, 40, 62}, 0, 0, {0, 0, 15, 35}, 40, 0, 0, JX(2, 0)},
    {PH, "DIGI PIANO", {1, 3, 34, 72, 0, 0, 0, 0}, {0, 96, 24, 60}, 0, 0, {0, 0, 10, 40}, 40, 0, 0, JX(1, 0)},
    {PH, "PD PLUCK", {5, 0, 30, 92, 0, 0, 0, 0}, {0, 70, 0, 50}, 0, 0, {0, 0, 35, 35}, 40, 0, 0, 0},
    {PH, "GLASS BELL", {6, 0, 70, 60, 24, 1, 0, 0}, {0, 100, 0, 92}, 0, 0, {0, 0, 30, 70}, 40, 0, 0, JX(2, 0)},
    {PH, "MALLET", {3, 0, 40, 84, 0, 0, 0, 0}, {0, 74, 0, 44}, 0, 0, {0, 0, 20, 40}, 40, 0, 0, 0},
    {PH, "PD PAD", {0, 4, 36, 30, 10, 0, 20, 0}, {80, 100, 112, 100}, 0, 0, {0, 0, 25, 70}, 40, 0, 0, JX(2, 0)},
    {PH, "FOLD LEAD", {0, 0, 50, 40, 8, 0, 0, 60}, {2, 70, 100, 40}, 0, 1, {0, 0, 35, 25}, 40, 0, 0, 0},
    {PH, "SOLO SQR", {1, 0, 46, 30, 4, 0, 0, 0}, {4, 70, 100, 40}, 0, 1, {0, 0, 40, 25}, 40, 0, 0, 0},
    /* TRIO: {WAVE, INT2, INT3, DTN, MODE, CUT, RES, PW} (WAVE: 0 SAW3 1 PLS3 2 PPT 3 SST 8 NOIS 12 RING ..) */
    {TR, "CHIP BASS", {1, 0, -12, 0, 0, 60, 30, 40}, {0, 50, 60, 20}, 30, 1, {6, 0, 0, 5}, 40, 0, 0, 0},
    {TR, "WOBBLE BASS", {0, 0, -12, 6, 0, 50, 70, 64}, {0, 70, 90, 20}, 20, 1, {10, 0, 0, 5}, 60, 0, 40, 0},
    {TR, "CHIP PLUCK", {1, 12, 0, 0, 0, 80, 20, 32}, {0, 44, 0, 30}, 20, 0, {0, 0, 35, 25}, 40, 0, 0, 0},
    {TR, "CHIP KEYS", {2, 0, 12, 4, 0, 74, 16, 48}, {0, 80, 50, 40}, 10, 0, {0, 0, 20, 30}, 40, 0, 0, JX(1, 0)},
    {TR, "TOY PIANO", {12, 0, 19, 0, 0, 92, 10, 64}, {0, 70, 0, 50}, 0, 0, {0, 0, 25, 40}, 40, 0, 0, 0},
    {TR, "8BIT PAD", {3, 0, 7, 10, 0, 58, 20, 64}, {70, 90, 110, 90}, 10, 0, {0, 0, 25, 65}, 30, 0, 6, JX(2, 0)},
    {TR, "NOISE PAD", {8, 0, 0, 0, 1, 60, 80, 64}, {90, 100, 100, 100}, 30, 0, {0, 0, 20, 80}, 16, 0, 24, 0},
    {TR, "CHIP LEAD", {1, 0, 0, 0, 0, 90, 10, 24}, {0, 60, 100, 30}, 0, 1, {0, 0, 35, 20}, 70, 20, 0, 0},
    /* ACID: {WAVE, CUT, RES, ENV, DECY, ACC, DRV, KTR}; one voice, LEGATO */
    {AC, "ACID LEAD", {0, 50, 90, 60, 60, 80, 40, 64}, {0, 64, 100, 24}, 0, 1, {10, 0, 40, 20}, 40, 0, 0, 0},
    {AC, "SQUELCH LEAD", {1, 44, 112, 90, 50, 100, 60, 64}, {0, 64, 100, 22}, 0, 1, {20, 0, 35, 15}, 40, 0, 0, 0},
    {AC, "ACID PLUCK", {1, 40, 60, 70, 30, 60, 10, 64}, {0, 50, 0, 30}, 0, 1, {0, 0, 40, 25}, 40, 0, 0, 0},
    {AC, "SOFT BASS", {1, 32, 40, 44, 70, 40, 12, 32}, {0, 64, 100, 20}, 0, 1, {0, 0, 0, 5}, 40, 0, 0, 0},
};
#undef J
#undef PH
#undef TR
#undef AC
#define NCC_NSND NELEM(NCC_SND)

/* NCC sound k on track t (switched to its engine first) */
static void ncc_snd_load(track_t *t, uint32_t k)
{
    const ncc_snd_t *j = &NCC_SND[k % NCC_NSND];
    uint32_t i;
    load_begin(t, UNDO_SOUND);
    if (t->eng_req != j->eng)
        set_engine_of(t, j->eng);
    apply_preset_to(t, 0);                              /* every value at the engine's defaults (its sends, ENV, LFO ..) */
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = j->e[i];
    t->p[P_ATK] = j->env[0];
    t->p[P_DEC] = j->env[1];
    t->p[P_SUS] = j->env[2];
    t->p[P_REL] = j->env[3];
    t->p[P_ED_FLT] = j->fenv;
    t->p[P_VOICE] = j->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = j->fx[i];
    t->p[P_LRATE] = j->lrate;
    t->p[P_LD_SHP] = j->lshp;
    t->p[P_LD_FLT] = j->lflt;
    t->p[P_ED_FX] = j->jx;
    if (j->eng == ENGI_ACID) {                          /* (ACID: the slide, as live_ui.c gives a new ACID sound) */
        t->p[P_GLMODE] = 1;
        if (!t->p[P_GLIDE])
            t->p[P_GLIDE] = LV_ACID_GLIDE;
    }
    load_end(t);
    if ((uint32_t)(t - trk) < NCM_PARTS)
        ncm_gen[t - trk]++;                             /* the macros back to the middle (perf_layer.c) */
}
