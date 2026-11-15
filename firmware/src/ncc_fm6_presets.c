/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): NCC's own FM6 patches, for its FM6 sounds (ncc_presets.c: an FM6 entry's last value is its
 * patch here). Made for NCC as operator settings (no factory ROM data of any instrument), in the packed 128-byte
 * voice layout FM6's factory patches and the voice bank have (eng_fm6.c fm6_unpack; tools/gen_fm6_patches.py pack).
 * A sound load puts the patch in as the part's own (SLOT OWN, as a user preset's or the editor's), so it is stored
 * with the project and SAVE keeps it. Included by ncc_presets.c. */

/* an operator: rates R1..R4, levels L1..L4, output level, frequency coarse (0: 0.5) and fine (0..99: + 1 % each),
 * detune (7: none), key velocity sensitivity (0..7), rate scaling (0..7), amplitude modulation sensitivity (0..3:
 * 0 in every patch here, no tremolo: fm6_core.c's AMS step overflows with it on, found 2026-11-15, not yet fixed).
 * Ratio mode, no level scaling */
#define NF_OP(r1, r2, r3, r4, l1, l2, l3, l4, ol, fc, ff, det, kvs, rs, ams)                                        \
    r1, r2, r3, r4, l1, l2, l3, l4, 39, 0, 0, 0, ((rs) & 7) | ((det) & 15) << 3, ((ams) & 3) | ((kvs) & 7) << 2, ol, \
    ((fc) & 31) << 1, ff
#define NF_OFF NF_OP(99, 99, 99, 99, 99, 99, 99, 0, 0, 1, 0, 7, 0, 0, 0)       /* an operator not used */
/* the voice's end: algorithm 1..32, feedback, the LFO's speed, delay, pitch depth, amplitude depth, wave (0 TRI ..
 * 4 SINE), pitch sensitivity; NF_ENDP with the pitch envelope's rates and levels before them (NF_END: none). The
 * name's ten bytes are the sound's (ncc_snd_load) */
#define NF_ENDP(alg, fb, pr1, pr2, pr3, pr4, pl1, pl2, pl3, pl4, lfs, lfd, lpmd, lamd, lfw, lpms)                   \
    pr1, pr2, pr3, pr4, pl1, pl2, pl3, pl4, (alg) - 1, ((fb) & 7) | 1 << 3, lfs, lfd, lpmd, lamd,                   \
    1 | ((lfw) & 7) << 1 | ((lpms) & 7) << 4, 24, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
#define NF_END(alg, fb, lfs, lfd, lpmd, lamd, lfw, lpms)                                                            \
    NF_ENDP(alg, fb, 99, 99, 99, 99, 50, 50, 50, 50, lfs, lfd, lpmd, lamd, lfw, lpms)

/* the record starts with OP6. Algorithm 5: three pairs (1 <- 2, 3 <- 4, 5 <- 6 with the feedback); 1: 1 <- 2 and
 * 3 <- 4 <- 5 <- 6 (feedback); 32: six carriers */
static const uint8_t NCC_FM6[][FM6_PACKED] = {
    {   /* 0 STAGE EP: a mellow body, a bark with velocity, a detuned pair under it */
        NF_OP(95, 45, 30, 60, 99, 78, 0, 0, 55, 1, 0, 7, 3, 1, 0),         /* 6: the third pair's modulator */
        NF_OP(95, 28, 20, 55, 99, 88, 0, 0, 84, 1, 0, 10, 1, 1, 0),        /* 5 */
        NF_OP(97, 70, 45, 65, 99, 50, 0, 0, 62, 14, 0, 7, 7, 3, 0),        /* 4: the tine */
        NF_OP(95, 24, 20, 50, 99, 92, 0, 0, 86, 1, 0, 6, 2, 2, 0),         /* 3 */
        NF_OP(95, 52, 34, 70, 99, 70, 0, 0, 68, 1, 0, 7, 7, 3, 0),         /* 2: the bark */
        NF_OP(96, 26, 24, 62, 99, 78, 0, 0, 97, 1, 0, 7, 2, 2, 0),         /* 1 */
        NF_END(5, 4, 30, 20, 0, 0, 4, 0)},
    {   /* 1 WURLY EP: a reed, shorter */
        NF_OFF,
        NF_OFF,
        NF_OP(98, 62, 40, 65, 99, 55, 0, 0, 64, 3, 0, 7, 6, 3, 0),         /* 4 */
        NF_OP(96, 38, 28, 62, 99, 70, 0, 0, 86, 1, 0, 8, 2, 2, 0),         /* 3 */
        NF_OP(96, 45, 30, 65, 99, 80, 0, 0, 80, 1, 0, 7, 5, 2, 0),         /* 2: the reed's buzz */
        NF_OP(97, 40, 28, 64, 99, 72, 0, 0, 98, 1, 0, 6, 2, 2, 0),         /* 1 */
        NF_END(5, 5, 38, 0, 0, 0, 0, 0)},
    {   /* 2 PERC ORGAN: hollow drawbars (1 2 4), a percussion at 3 and a key click */
        NF_OP(99, 92, 99, 90, 99, 0, 0, 0, 68, 12, 0, 7, 2, 0, 0),         /* 6: the click */
        NF_OP(99, 62, 99, 85, 99, 0, 0, 0, 86, 3, 0, 7, 1, 0, 0),          /* 5: the percussion */
        NF_OP(99, 99, 99, 86, 99, 99, 99, 0, 72, 8, 0, 7, 0, 0, 0),        /* 4 */
        NF_OP(99, 99, 99, 86, 99, 99, 99, 0, 78, 4, 0, 8, 0, 0, 0),        /* 3 */
        NF_OP(99, 99, 99, 86, 99, 99, 99, 0, 86, 2, 0, 6, 0, 0, 0),        /* 2 */
        NF_OP(99, 99, 99, 86, 99, 99, 99, 0, 92, 1, 0, 7, 0, 0, 0),        /* 1 */
        NF_END(32, 0, 62, 0, 4, 0, 4, 2)},
    {   /* 3 TUBE BELLS: low, long, the modulators at 3.5 */
        NF_OP(96, 50, 30, 40, 99, 55, 0, 0, 58, 7, 0, 7, 2, 2, 0),         /* 6 */
        NF_OP(96, 40, 26, 38, 99, 70, 0, 0, 72, 2, 0, 5, 1, 2, 0),         /* 5: the strike's shine */
        NF_OP(96, 30, 20, 36, 99, 78, 0, 0, 72, 3, 50, 4, 3, 1, 0),        /* 4 */
        NF_OP(95, 28, 18, 34, 99, 82, 0, 0, 92, 1, 0, 10, 1, 1, 0),        /* 3 */
        NF_OP(96, 30, 20, 36, 99, 80, 0, 0, 75, 3, 50, 7, 3, 1, 0),        /* 2 */
        NF_OP(95, 28, 18, 34, 99, 82, 0, 0, 95, 1, 0, 7, 1, 1, 0),         /* 1 */
        NF_END(5, 0, 35, 0, 0, 0, 0, 0)},
    {   /* 4 LATELY BASS: a stack with feedback on one carrier, a plain pair on the other: hard and woody */
        NF_OP(99, 60, 40, 72, 99, 80, 60, 0, 66, 1, 0, 7, 3, 2, 0),        /* 6 */
        NF_OP(99, 58, 38, 72, 99, 76, 58, 0, 72, 1, 0, 7, 3, 2, 0),        /* 5 */
        NF_OP(99, 62, 40, 72, 99, 72, 55, 0, 76, 2, 0, 7, 5, 2, 0),        /* 4 */
        NF_OP(99, 55, 35, 72, 99, 86, 70, 0, 93, 1, 0, 8, 2, 1, 0),        /* 3 */
        NF_OP(99, 64, 40, 72, 99, 70, 50, 0, 84, 1, 0, 7, 5, 2, 0),        /* 2 */
        NF_OP(99, 55, 35, 72, 99, 88, 72, 0, 98, 1, 0, 7, 2, 1, 0),        /* 1 */
        NF_END(1, 7, 35, 0, 0, 0, 0, 0)},
    {   /* 5 SOLID BASS: a punch that closes fast, a click, a sine an octave below */
        NF_OFF,
        NF_OP(99, 50, 35, 74, 99, 90, 78, 0, 90, 0, 0, 7, 1, 0, 0),        /* 5: the sub */
        NF_OP(99, 86, 50, 74, 99, 20, 0, 0, 70, 3, 0, 7, 6, 3, 0),         /* 4: the click */
        NF_OP(99, 52, 36, 74, 99, 84, 66, 0, 88, 1, 0, 6, 2, 1, 0),        /* 3 */
        NF_OP(99, 76, 42, 74, 99, 46, 30, 0, 86, 1, 0, 7, 5, 2, 0),        /* 2: the punch */
        NF_OP(99, 52, 36, 74, 99, 86, 70, 0, 98, 1, 0, 7, 2, 1, 0),        /* 1 */
        NF_END(5, 0, 35, 0, 0, 0, 0, 0)},
    {   /* 6 KOTO PLUCK: a hard pick (modulators at 3 and 5 that close at once), a twang beside it */
        NF_OP(99, 66, 40, 55, 99, 40, 0, 0, 68, 1, 0, 7, 4, 2, 0),         /* 6 */
        NF_OP(99, 55, 35, 50, 99, 45, 0, 0, 76, 2, 0, 7, 2, 2, 0),         /* 5 */
        NF_OP(99, 82, 45, 60, 99, 25, 0, 0, 72, 5, 0, 7, 6, 3, 0),         /* 4 */
        NF_OP(99, 50, 34, 50, 99, 50, 0, 0, 90, 1, 0, 9, 2, 2, 0),         /* 3 */
        NF_OP(99, 72, 40, 60, 99, 36, 0, 0, 80, 3, 0, 7, 5, 3, 0),         /* 2 */
        NF_OP(99, 50, 34, 50, 99, 52, 0, 0, 98, 1, 0, 7, 2, 2, 0),         /* 1 */
        NF_END(5, 5, 35, 0, 0, 0, 0, 0)},
    {   /* 7 SOFT BRASS: a horn: slow lips, less edge, a late vibrato */
        NF_OP(46, 45, 40, 52, 99, 86, 80, 0, 64, 1, 0, 7, 3, 0, 0),        /* 6 */
        NF_OP(54, 45, 40, 52, 99, 94, 90, 0, 84, 1, 0, 6, 1, 0, 0),        /* 5 */
        NF_OP(44, 45, 40, 52, 99, 86, 82, 0, 68, 1, 0, 7, 4, 0, 0),        /* 4 */
        NF_OP(54, 45, 40, 52, 99, 94, 90, 0, 90, 1, 0, 8, 1, 0, 0),        /* 3 */
        NF_OP(48, 45, 40, 52, 99, 88, 84, 0, 70, 1, 0, 7, 4, 0, 0),        /* 2 */
        NF_OP(56, 45, 40, 52, 99, 94, 90, 0, 97, 1, 0, 7, 1, 0, 0),        /* 1 */
        NF_ENDP(5, 6, 84, 60, 99, 60, 48, 50, 50, 50, 34, 60, 5, 0, 4, 3)},
};
#define NCC_NFM6 NELEM(NCC_FM6)
