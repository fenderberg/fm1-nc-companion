/* SPDX-License-Identifier: GPL-3.0-only */
/* Included inside NCM_MAP. Engine numbers are the stored ENGINES[] indices; no engine interface changes.
 * Model-dependent fields use show=0 so the popup reads the engine's current descriptor (BOW/BEND/BUZZ, etc.).
 * Keep source/model switching on EDIT; the live knobs shape the selected material. */
    {ENGI_PHYS, {
        {{P_E1, 32}, {NCM_NONE, 0}},              /* structure / harmonics / sympathetic chord */
        {{P_E4, 32}, {NCM_NONE, 0}},              /* strike position / sympathetic amount */
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},          /* envelope -> brightness */
        {{P_E2, 32}, {NCM_NONE, 0}},
        {{P_E3, 32}, {NCM_NONE, 0}},              /* longer ringing at higher DAMP */
        {{P_E6, 32}, {NCM_NONE, 0}},              /* bow / membrane bend / sympathetic buzz */
        {{P_DIST, 32}, {P_CHOR, 32}},
    }, {0, 0, "ATK+REL", "ENV AMT", "BRIGHT", "RING", 0, "DRIVE"}},
    {8, {
        {{P_E1, 32}, {NCM_NONE, 0}},              /* GRAIN: position */
        {{P_E2, 32}, {NCM_NONE, 0}},              /* grain size */
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},
        {{P_E7, 32}, {NCM_NONE, 0}},
        {{P_E3, 32}, {NCM_NONE, 0}},              /* density instead of filter resonance */
        {{P_E6, 32}, {P_E5, 32}},                 /* random pitch/reversal + position spread */
        {{P_DIST, 32}, {P_CHOR, 32}},
    }, {"POSITION", "SIZE", "ATK+REL", "ENV AMT", "TONE", "DENSITY", "SPRAY", "DRIVE"}},
    {5, {
        {{P_E0, 32}, {P_E1, 32}},                 /* VOICE: move both vowels, preserving the transition */
        {{P_E4, 32}, {NCM_NONE, 0}},
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},
        {{P_E3, 32}, {NCM_NONE, 0}},              /* shift all formant frequencies */
        {{P_E6, 32}, {NCM_NONE, 0}},
        {{P_LD_FLT, 32}, {P_LRATE, 32}},
        {{P_DIST, 32}, {P_CHOR, 32}},
    }, {"VOWELS", "BUZZ", "ATK+REL", "VOW ENV", "FORMANT", "Q", "LFO", "DRIVE"}},
    {3, {
        {{P_E1, 32}, {NCM_NONE, 0}},              /* LOFI: wave */
        {{P_E2, 32}, {NCM_NONE, 0}},              /* duty / wave RAM; ncm_destination selects SWP for the other waves */
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},
        {{P_E7, 32}, {NCM_NONE, 0}},
        {{P_E3, 32}, {NCM_NONE, 0}},              /* crush / STEP envelope */
        {{P_E5, 32}, {NCM_NONE, 0}},
        {{P_DIST, 32}, {P_CHOR, 32}},
    }, {"WAVE", 0, "ATK+REL", "ENV AMT", "TONE", 0, "VIBRATO", "DRIVE"}},
    {7, {
        {{P_E0, 64}, {NCM_NONE, 0}},              /* WHEEL: drawbar registration */
        {{P_E1, 32}, {NCM_NONE, 0}},
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_E4, 64}, {NCM_NONE, 0}},              /* real percussion; this engine has no cutoff modulation */
        {{P_E3, 32}, {NCM_NONE, 0}},
        {{P_E2, 32}, {NCM_NONE, 0}},
        {{P_E7, 127}, {NCM_NONE, 0}},             /* OFF / SLOW / FAST */
        {{P_E6, 32}, {NCM_NONE, 0}},
    }, {"BARS", "SUB", "ATK+REL", "PERC", "TOP", "BODY", "ROTOR", "DRIVE"}},
    {11, {
        {{P_E1, 32}, {NCM_NONE, 0}},              /* NOISE: color */
        {{P_E5, 32}, {NCM_NONE, 0}},              /* dust density / shift register */
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},
        {{P_E2, 32}, {NCM_NONE, 0}},
        {{P_E3, 32}, {NCM_NONE, 0}},
        {{P_LD_FLT, 32}, {P_LRATE, 32}},
        {{P_DIST, 32}, {P_CHOR, 32}},
    }, {"COLOR", 0, "ATK+REL", "ENV AMT", "CUTOFF", "RESO", "LFO", "DRIVE"}},
    {ENGI_SAMPLE, {
        {{P_E1, 16}, {NCM_NONE, 0}},              /* tune: about +/-6 semitones */
        {{P_E2, 32}, {NCM_NONE, 0}},
        {{P_ATK, 32}, {P_REL, 32}},
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},
        {{P_E4, 32}, {NCM_NONE, 0}},
        {{P_E6, 32}, {NCM_NONE, 0}},              /* drive instead of nonexistent resonance */
        {{P_LD_PIT, 2}, {P_LRATE, 32}},
        {{P_CHOR, 32}, {NCM_NONE, 0}},
    }, {"TUNE", "BITS", "ATK+REL", "ENV AMT", "CUTOFF", "DRIVE", "VIBRATO", "CHORUS"}},
