/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the macro layer. The Circuit Tracks' MIDI tracks send its 8
 * macro knobs as the CCs of their template; the factory template's are NCM_CC (1 2 5 11 12 13 71 74, knob 1..8). On
 * the channel of an NCC part (tracks 1 and 2: L FM6, R ANALOG; MIDI IN TRACK, channels 3 / 4) they are the macros,
 * before Felucca's own meanings (midi_event: CC1 the wheel, CC11 EXPR, then CC5 GLIDE, the standard map, MIDI LEARN).
 * Each knob follows the Circuit's panel legend (NCM_NAME) with up to two parameters of the part's engine (NCM_MAP).
 *
 * Relative to the sound: CC 64 is the sound as loaded; 0 / 127 move each parameter by its amount (a64: 1/64 of the
 * parameter's range at full turn) down / up from there, clamped to its range. A new sound (engine, preset, user preset,
 * FM6 patch) puts every knob of the part back to 64 at once; the next CC slides there from 64. A value written by
 * anything else (a sound load, Program Change, the editor) becomes the parameter's base (ncm_block compares it with
 * what it last wrote).
 *
 * Audio ISR only: ncm_cc (midi_control.c midi_event, in events_block) sets a knob's target, the last CC of a block wins;
 * ncm_block (fx.c mix_block, right after events_block) slides each knob a quarter of the way to its target per block
 * (32 samples, 0.73 ms: 95 % in ~10 ms, no zipper) and writes a parameter only when its value changes. The main loop
 * only reads ncm_seen (the popup: live_ui.c). */

#ifndef ENGI_ANALOG
#define ENGI_ANALOG 0u                  /* engines.c ENGINES[] */
#endif
#define NCM_PARTS 2u                    /* tracks 1, 2 */
#define NCM_N 8u                        /* knobs */
#define NCM_DST 2u                      /* parameters a knob moves */
#define NCM_NONE 0xFFu
#define NCM_CEG 0xFEu                   /* FM6's carrier envelope times (eng_fm6.c fm6_ceg: not a stored parameter) */
#define NCM_MID (64 << 8)               /* a knob's middle, Q8 */
#define NCM_SLEW 2                      /* per block: a 1/4 of the way (time constant 4 blocks, 2.9 ms) */
#define NCM_LAZY 3u                     /* FM6's patch values: written while sliding every 4th block (and on arrival) */
#define NCM_UNSET (-32768)

static const uint8_t NCM_CC[NCM_N] = {1, 2, 5, 11, 12, 13, 71, 74};
static const char *const NCM_NAME[NCM_N] = {"OSCILLATOR", "OSC MOD", "AMP ENV", "FILTER ENV", "FILTER FREQ",
                                            "RESONANCE", "MODULATION", "FX"};
typedef struct {
    uint8_t id;                         /* P_* id, NCM_CEG, NCM_NONE */
    int8_t a64;                         /* amount: 1/64 of the parameter's range at full turn */
} ncm_dst_t;
typedef struct {
    uint8_t eng;                        /* ENGINES[] index */
    ncm_dst_t d[NCM_N][NCM_DST];
} ncm_map_t;
/* the Circuit's legends on the two engines (docs/NC-mijlpaal-1-plan.md) */
static const ncm_map_t NCM_MAP[] = {
    {ENGI_FM6, {
        {{P_E3, 16}, {NCM_NONE, 0}},              /* OSCILLATOR   MRAT, the modulators' ratios (steps): +-8 */
        {{P_E1, 32}, {NCM_NONE, 0}},              /* OSC MOD      FB, feedback (steps): +-7 */
        {{NCM_CEG, 32}, {NCM_NONE, 0}},           /* AMP ENV      the carriers' envelope times: + slower */
        {{P_E4, 32}, {NCM_NONE, 0}},              /* FILTER ENV   MEG, the modulators' envelope times */
        {{P_E2, 32}, {NCM_NONE, 0}},              /* FILTER FREQ  MLVL, the brightness */
        {{P_E6, 64}, {NCM_NONE, 0}},              /* RESONANCE    DTUN, the carriers apart (FM6 has no filter) */
        {{P_LD_FLT, 32}, {P_LD_PIT, 8}},          /* MODULATION   the LFO on the brightness, a little vibrato */
        {{P_DIST, 32}, {P_CHOR, 32}},             /* FX           drive, chorus send */
    }},
    {ENGI_ANALOG, {
        {{P_E2, 32}, {NCM_NONE, 0}},              /* OSCILLATOR   MIX, osc 1 / osc 2 */
        {{P_E1, 32}, {NCM_NONE, 0}},              /* OSC MOD      DTN, osc 2's detune */
        {{P_ATK, 32}, {P_REL, 32}},               /* AMP ENV      attack and release */
        {{P_ED_FLT, 32}, {NCM_NONE, 0}},          /* FILTER ENV   ENV -> FLT */
        {{P_E4, 32}, {NCM_NONE, 0}},              /* FILTER FREQ  CUT */
        {{P_E5, 32}, {NCM_NONE, 0}},              /* RESONANCE    RES */
        {{P_LD_FLT, 32}, {P_LRATE, 32}},          /* MODULATION   the LFO on the filter, its rate */
        {{P_DIST, 32}, {P_CHOR, 32}},             /* FX           drive, chorus send */
    }},
};

static struct {
    uint8_t tgt[NCM_PARTS][NCM_N];      /* the last CC, 0..127 */
    uint16_t cur[NCM_PARTS][NCM_N];     /* where the knob is, Q8 */
    int16_t base[NCM_PARTS][NCM_N][NCM_DST];    /* the parameter as the sound has it */
    int16_t put[NCM_PARTS][NCM_N][NCM_DST];     /* what ncm_block last wrote (NCM_UNSET: nothing yet) */
    uint32_t snd[NCM_PARTS];            /* the sound's signature (ncm_sig) */
    uint8_t gen[NCM_PARTS];             /* the ncm_gen seen */
    uint32_t n;                         /* blocks */
    uint8_t ok;
} ncm;
static volatile uint32_t ncm_seen;      /* the last knob moved: part << 16 | knob << 8 | value, + 1 << 24 a turn */
static volatile uint8_t ncm_throw;      /* main (live_ui.c): bit 0 ENV held = ECHO THROW, bit 1 LFO held = REV THROW */
static uint8_t ncm_thr;                 /* .. as the ISR last pressed them (perform.c perf_press: the ISR's alone) */
static volatile uint8_t ncm_gen[NCM_PARTS];     /* the main loop: + 1 = a new sound the signature cannot see (live_ui.c:
                                                 * a Juno preset over ANALOG's preset 0) */

static const ncm_map_t *ncm_map(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NELEM(NCM_MAP); i++)
        if (NCM_MAP[i].eng == eng_idx(t->eng_req))
            return &NCM_MAP[i];
    return 0;
}
static uint32_t ncm_sig(const track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk);
    return eng_idx(t->eng_req) | (uint32_t)t->preset << 8 | (uint32_t)t->user << 16 | (uint32_t)fm6_pgen[tr] << 24;
}
static int32_t ncm_get(track_t *t, uint32_t id)
{
    return id == NCM_CEG ? fm6_ceg[t - trk] : t->p[id];
}
static void ncm_set(track_t *t, uint32_t id, int32_t v)
{
    if (id == NCM_CEG)
        fm6_ceg[t - trk] = (int8_t)v;
    else
        t->p[id] = (int16_t)v;
}
/* a value FM6 rebuilds its patch through (fm6_sync: a copy of the patch and the six operators): while a knob slides
 * it is written every 4th block only, so a turn costs a rebuild per 2.9 ms, not per block. MLVL is read per block
 * as it is (fm6_render) */
static int ncm_lazy(const track_t *t, uint32_t id)
{
    return t->eng_req == ENGI_FM6 && (id == NCM_CEG || (id >= P_E0 && id <= P_E6 && id != P_E2));
}
static void ncm_range(const track_t *t, uint32_t id, int32_t *lo, int32_t *hi)
{
    const param_desc_t *d;
    if (id == NCM_CEG) {
        *lo = -64;
        *hi = 63;
        return;
    }
    d = param_desc_of(eng_idx(t->eng_req), id);
    *lo = d->min;
    *hi = d->max;
}
static void ncm_reset(uint32_t part)    /* every knob of the part at 64, nothing written yet */
{
    uint32_t m, k;
    for (m = 0; m < NCM_N; m++) {
        ncm.tgt[part][m] = 64;
        ncm.cur[part][m] = NCM_MID;
        for (k = 0; k < NCM_DST; k++)
            ncm.put[part][m][k] = NCM_UNSET;
    }
    fm6_ceg[part] = 0;
}

/* midi_event: CC cc on track t; 1 when it is a macro */
static int ncm_cc(track_t *t, uint32_t cc, uint32_t value)
{
    uint32_t part = (uint32_t)(t - trk), m;
    if (part >= NCM_PARTS || !ncm_map(t))
        return 0;
    for (m = 0; m < NCM_N && NCM_CC[m] != cc; m++)
        ;
    if (m == NCM_N)
        return 0;
    ncm.tgt[part][m] = (uint8_t)(value & 127u);
    ncm_seen = ((ncm_seen >> 24) + 1u) << 24 | part << 16 | m << 8 | (value & 127u);
    return 1;
}

/* fx.c mix_block, once a block after events_block: the knobs slide, their parameters follow */
static void ncm_block(void)
{
    uint32_t part, m, k;
    int there;
    if (!ncm.ok) {
        for (part = 0; part < NCM_PARTS; part++) {
            ncm_reset(part);
            ncm.snd[part] = ncm_sig(&trk[part]);
        }
        ncm.ok = 1;
    }
    ncm.n++;
    if (ncm_throw != ncm_thr) {                         /* the throws on ENV / LFO: PERFORM's, held while held */
        uint32_t ch = ncm_throw ^ ncm_thr;
        if (ch & 1u)
            perf_press(PF_ETHR, ncm_throw & 1u);
        if (ch & 2u)
            perf_press(PF_RTHR, (ncm_throw >> 1) & 1u);
        ncm_thr = ncm_throw;
    }
    for (part = 0; part < NCM_PARTS; part++) {
        track_t *t = &trk[part];
        const ncm_map_t *mp = ncm_map(t);
        uint32_t sig = ncm_sig(t);
        if (sig != ncm.snd[part] || ncm.gen[part] != ncm_gen[part]) {   /* a new sound: the knobs back to the middle */
            ncm.snd[part] = sig;
            ncm.gen[part] = ncm_gen[part];
            ncm_reset(part);
        }
        if (!mp)
            continue;
        for (m = 0; m < NCM_N; m++) {
            int32_t c = ncm.cur[part][m], d = ((int32_t)ncm.tgt[part][m] << 8) - c, s = d >> NCM_SLEW;
            if (!d && c == NCM_MID)                         /* at rest in the middle: the sound's values (a write by */
                continue;                                   /* anyone else meanwhile is the base when it moves: put) */
            c = s ? c + s : c + d;                          /* (the last 1/4 steps: there) */
            ncm.cur[part][m] = (uint16_t)c;
            there = c == (int32_t)ncm.tgt[part][m] << 8;
            for (k = 0; k < NCM_DST; k++) {
                const ncm_dst_t *ds = &mp->d[m][k];
                int32_t lo, hi, v, now;
                if (ds->id == NCM_NONE)
                    continue;
                now = ncm_get(t, ds->id);
                if (now == ncm.put[part][m][k] && !d)       /* a knob at rest off the middle, its value ours: done */
                    continue;
                if (now != ncm.put[part][m][k])             /* written by someone else (or never by us): the base */
                    ncm.base[part][m][k] = (int16_t)now;
                ncm_range(t, ds->id, &lo, &hi);
                v = ncm.base[part][m][k] + (((c - NCM_MID) * (hi - lo) * ds->a64) >> 20);   /* (64 << 8) * 64 */
                v = clamp(v, lo, hi);
                if (v != now) {
                    if (!there && (ncm.n & NCM_LAZY) && ncm_lazy(t, ds->id))
                        continue;                           /* (later: put stays what the patch has) */
                    ncm_set(t, ds->id, v);
                }
                ncm.put[part][m][k] = (int16_t)v;
            }
        }
    }
}
