/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the Juno sounds on ANALOG (main loop; live_ui.c lists them first on PRESETS). ANALOG plays
 * a Juno's voice with WAVE SUB (OSC 1 a saw, OSC 2 a square an octave below it, MIX its level: the DCO and its sub) or
 * PWM (the LFO on SHP sweeps the width), its SVF low-pass and ADSR; the Juno chorus and the drift are P_ED_FX
 * (juno_chorus.c: chorus | drift << 2). Every sound here has some drift, so P_ED_FX >= 4 marks a Juno sound after a
 * power-on (the column then says JUNO: which one is not stored). Loaded as one sound load: ANALOG's preset 0 for the
 * defaults of everything, then these values, inside one load_begin / load_end (one undo, the fades of a load). */

typedef struct {
    const char *name;
    int8_t e[8];                        /* WAVE DTN MIX NOIS CUT RES DRV KTR */
    uint8_t env[4];                     /* ATK DEC SUS REL */
    int8_t fenv;                        /* ENV -> FILTER */
    uint8_t mono;                       /* 1: LEGATO, 0: POLY */
    uint8_t fx[4];                      /* DIST, CHORUS (the bus), DELAY, REVERB sends */
    uint8_t lrate;                      /* LFO RATE */
    int8_t lshp, lflt;                  /* LFO -> SHP (PWM's width), LFO -> FLT */
    uint8_t jx;                         /* P_ED_FX: Juno chorus (0 OFF, 1 I, 2 II, 3 I+II) | drift (0..15) << 2 */
} ncc_juno_t;

#define JX(chorus, drift) (uint8_t)((chorus) | (drift) << 2)
#define NJ_SAW 0
#define NJ_SQR 1
#define NJ_PWM 4
#define NJ_SUB 6                       /* (eng_analog.c AW_SUB) */
static const ncc_juno_t NCC_JUNO[] = {
    {"JUNO STRINGS", {NJ_PWM, 0, 0, 0, 72, 6, 0, 48}, {55, 90, 112, 80}, 4, 0, {0, 0, 10, 45}, 34, 26, 0, JX(2, 6)},
    {"JUNO PAD", {NJ_SUB, 4, 40, 4, 56, 14, 0, 40}, {92, 100, 116, 100}, 10, 0, {0, 0, 20, 65}, 18, 0, 6, JX(1, 8)},
    {"JUNO BRASS", {NJ_SAW, 0, 0, 0, 40, 16, 0, 64}, {26, 70, 96, 40}, 34, 0, {0, 0, 10, 30}, 40, 0, 0, JX(1, 4)},
    {"POLY SYNTH", {NJ_SUB, 0, 30, 0, 60, 24, 0, 64}, {2, 76, 70, 50}, 24, 0, {0, 0, 25, 35}, 40, 0, 0, JX(1, 5)},
    {"JUNO ORGAN", {NJ_SQR, 0, 0, 0, 92, 0, 0, 64}, {0, 10, 127, 14}, 0, 0, {0, 0, 0, 30}, 40, 0, 0, JX(3, 2)},
    {"JUNO BASS", {NJ_SUB, 0, 70, 0, 34, 36, 10, 64}, {0, 60, 40, 20}, 30, 1, {8, 0, 0, 10}, 40, 0, 0, JX(0, 1)},
    {"SUB BASS", {NJ_SUB, 0, 127, 0, 30, 0, 0, 32}, {0, 50, 110, 16}, 6, 1, {0, 0, 0, 5}, 40, 0, 0, JX(0, 1)},
    {"JUNO PLUCK", {NJ_SAW, 0, 0, 0, 30, 30, 0, 64}, {0, 56, 0, 46}, 44, 0, {0, 0, 30, 25}, 40, 0, 0, JX(2, 4)},
    {"JUNO LEAD", {NJ_SUB, 0, 20, 0, 70, 30, 0, 64}, {2, 70, 100, 36}, 14, 1, {0, 0, 35, 25}, 40, 0, 0, JX(1, 3)},
    {"HOOVER", {NJ_PWM, 0, 0, 0, 80, 10, 30, 64}, {6, 80, 110, 50}, 8, 1, {20, 0, 15, 30}, 70, 50, 0, JX(3, 10)},
    {"JUNO BELLS", {NJ_SQR, 0, 0, 0, 100, 40, 0, 96}, {0, 70, 0, 80}, 10, 0, {0, 0, 25, 60}, 40, 0, 0, JX(2, 3)},
    {"JUNO SWEEP", {NJ_SAW, 0, 0, 0, 20, 70, 0, 64}, {80, 110, 100, 90}, 50, 0, {0, 0, 20, 60}, 14, 0, 26, JX(1, 6)},
};
#define NCC_NJUNO NELEM(NCC_JUNO)

/* Juno sound k on track t (ANALOG: switched to it first) */
static void ncc_juno_load(track_t *t, uint32_t k)
{
    const ncc_juno_t *j = &NCC_JUNO[k % NCC_NJUNO];
    uint32_t i;
    load_begin(t, UNDO_SOUND);
    if (t->eng_req != ENGI_ANALOG)
        set_engine_of(t, ENGI_ANALOG);
    apply_preset_to(t, 0);                              /* every value at ANALOG's defaults (its sends, ENV, LFO ..) */
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
    load_end(t);
    if ((uint32_t)(t - trk) < NCM_PARTS)
        ncm_gen[t - trk]++;                             /* the macros back to the middle (perf_layer.c) */
}
