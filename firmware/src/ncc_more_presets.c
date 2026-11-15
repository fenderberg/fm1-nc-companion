/* SPDX-License-Identifier: GPL-3.0-only */
/* NC: 27 more synth sounds (0.8), included in NCC_SND after the extra engines' (append-only sound IDs): what the list
 * was thin on, and WAVE's that move by the LFO (eng_wave.c has its plain ones). FM6's are NCC's own patches
 * (ncc_fm6_presets.c); the others the engines as they are. Delay and reverb start dry; no samples or external voice
 * banks added. */
    /* FM6: {ALG (0: the patch's), FB, MLVL, MRAT, MEG, VMOD, DTUN, the patch in NCC_FM6}; ENV as FM6's presets */
    {12, "STAGE EP", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 127, 0}, 0, 0, {0, 20, 0, 0}, 40, 0, 0, 0},
    {12, "WURLY EP", {0, 0, 0, 0, 0, 0, 0, 1}, {0, 0, 127, 0}, 0, 0, {0, 0, 0, 0}, 40, 0, 0, 0},
    {12, "PERC ORGAN", {0, 0, 0, 0, 0, 0, 0, 2}, {0, 0, 127, 0}, 0, 0, {6, 20, 0, 0}, 40, 0, 0, 0},
    {12, "TUBE BELLS", {0, 0, 0, 0, 0, 0, 0, 3}, {0, 0, 127, 0}, 0, 0, {0, 0, 0, 0}, 40, 0, 0, 0},
    {12, "LATELY BASS", {0, 0, 0, 0, 0, 0, 0, 4}, {0, 0, 127, 0}, 0, 1, {0, 0, 0, 0}, 40, 0, 0, 0},
    {12, "SOLID BASS", {0, 0, 0, 0, 0, 0, 0, 5}, {0, 0, 127, 0}, 0, 1, {0, 0, 0, 0}, 40, 0, 0, 0},
    {12, "KOTO PLUCK", {0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 127, 0}, 0, 0, {0, 0, 0, 0}, 40, 0, 0, 0},
    {12, "SOFT BRASS", {0, 0, 0, 0, 0, 0, 0, 7}, {0, 0, 127, 0}, 0, 0, {0, 16, 0, 0}, 40, 0, 0, 0},
    /* JUNO: {WAVE, DTN, MIX, NOIS, CUT, RES, DRV, KTR} */
    {0, "SOLINA", {NJ_SAW, 12, 64, 0, 78, 4, 0, 56}, {48, 90, 116, 76}, 2, 0, {0, 30, 0, 0}, 44, 0, 0, JX(3, 8)},
    {0, "SUPER SAW", {NJ_SAW, 22, 64, 0, 96, 12, 8, 64}, {1, 70, 110, 44}, 6, 0, {0, 40, 0, 0}, 40, 0, 0, JX(3, 12)},
    {0, "WIDE PAD", {NJ_SUB, 10, 30, 3, 50, 18, 0, 44}, {96, 104, 118, 104}, 14, 0, {0, 25, 0, 0}, 16, 0, 8, JX(3, 10)},
    {0, "BRASS STAB", {NJ_SAW, 8, 64, 0, 44, 20, 6, 64}, {4, 52, 30, 26}, 46, 0, {0, 0, 0, 0}, 40, 0, 0, JX(1, 5)},
    {0, "REESE BASS", {NJ_SAW, 26, 64, 0, 38, 24, 14, 50}, {0, 70, 116, 22}, 10, 1, {6, 0, 0, 0}, 20, 0, 6, JX(0, 3)},
    {0, "PWM BASS", {NJ_PWM, 0, 0, 0, 42, 28, 8, 56}, {0, 62, 80, 18}, 26, 1, {4, 0, 0, 0}, 48, 34, 0, JX(0, 2)},
    {0, "SOFT LEAD", {NJ_TRI, 5, 40, 0, 84, 8, 0, 64}, {6, 70, 108, 42}, 6, 1, {0, 20, 0, 0}, 46, 0, 0, JX(1, 4)},
    /* PHASE: {WAVE, WAVE2, DCW, ENV, DTN, LINE, SUB, FOLD}: the resonant waves (RSAW RTRI RTRP) */
    {2, "RESO SWEEP", {5, 0, 30, 70, 8, 0, 0, 0}, {70, 110, 100, 96}, 0, 0, {0, 20, 0, 0}, 14, 20, 0, JX(2, 0)},
    {2, "RESO LEAD", {7, 0, 62, 50, 6, 0, 0, 0}, {2, 74, 100, 36}, 0, 1, {0, 0, 0, 0}, 44, 0, 0, 0},
    {2, "CZ BASS", {5, 1, 40, 96, 0, 0, 50, 0}, {0, 56, 50, 18}, 0, 1, {4, 0, 0, 0}, 40, 0, 0, 0},
    {2, "PD CLAV", {2, 0, 52, 88, 0, 0, 0, 10}, {0, 50, 0, 24}, 0, 0, {0, 0, 0, 0}, 40, 0, 0, 0},
    /* PHYS: MODEL STRC BRIT DAMP POS ACC BOW EXC */
    {9, "CELLO PAD", {1, 40, 60, 104, 30, 80, 96, 0}, {60, 100, 120, 84}, 6, 0, {0, 14, 0, 0}, 26, 0, 4, 0},
    {9, "KALIMBA", {0, 40, 70, 62, 30, 96, 0, 20}, {0, 100, 127, 50}, 0, 0, {0, 0, 0, 0}, 40, 0, 0, 0},
    /* VOICE: VOWL VOWL2 TALK SHIFT BUZZ BREATH Q RAND */
    {5, "CHOIR PAD", {0, 96, 100, 0, 30, 34, 60, 10}, {84, 100, 118, 100}, 0, 0, {0, 20, 0, 0}, 22, 0, 6, 0},
    {5, "VOX LEAD", {40, 10, 40, 2, 78, 8, 80, 0}, {4, 72, 108, 36}, 12, 1, {0, 0, 0, 0}, 44, 0, 6, 0},
    /* GRAIN: SRC POS SIZE DENS PTCH SPRD RAND TONE */
    {8, "GLASS CLOUD", {0, 70, 60, 110, 12, 60, 30, 118}, {60, 100, 116, 110}, 0, 0, {0, 20, 0, 0}, 20, 0, 0, 0},
    /* WAVE: {TABLE, POS, ENV, DTN, CUT, RES, SUB, KTR}: the LFO scans the table (LFO -> SHP) */
    {16, "SCAN PAD", {1, 64, 0, 10, 96, 6, 0, 56}, {80, 100, 116, 100}, 4, 0, {0, 30, 0, 0}, 12, 50, 0, JX(2, 6)},
    {16, "ICE PAD", {2, 40, 0, 14, 100, 0, 0, 64}, {64, 100, 112, 104}, 0, 0, {0, 20, 0, 0}, 18, 40, 0, JX(3, 5)},
    {16, "TALK LEAD", {1, 50, 0, 0, 104, 10, 0, 64}, {3, 72, 106, 34}, 0, 1, {0, 0, 0, 0}, 60, 45, 0, 0},
