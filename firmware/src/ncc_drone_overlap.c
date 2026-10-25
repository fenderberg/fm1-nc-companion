/* SPDX-License-Identifier: GPL-3.0-only */
/* REPLACE in POLY: keep rendering the old chord while free voices play the new one.
 * The ordinary shared/engine voice budgets remain hard limits. Event-core ramps
 * are read by either render core, exactly like the part-level fallback fade. */
#define NCD_RISE_BLOCKS 110u                       /* ~80 ms minimum entrance; patch attack remains intact */
static struct {
    uint16_t gain, from, pos;
    uint8_t phase;
} ncd_overlap[NCD_FADE_PARTS][NVOICE];
static uint8_t ncd_overlapping[NCD_FADE_PARTS];

static uint32_t ncd_overlap_gain(uint32_t part, uint32_t i)
{
    return ncd_overlap[part][i].phase ? ncd_overlap[part][i].gain : NCD_FADE_FULL;
}
static int ncd_retiring(const track_t *t, const voice_t *v)
{
    uint32_t part = (uint32_t)(t - trk);
    return part < NCD_FADE_PARTS && ncd_overlap[part][v - t->v].phase == NCD_FADE_OUT;
}
static int ncd_overlap_room(const track_t *t, uint32_t count)
{
    uint32_t i, free = 0;
    if (trk_vmode(t) != V_POLY || t->p[P_AMODE] || t->xf_on || t->engine != t->eng_req)
        return 0;
    for (i = 0; i < trk_nvoice(t); i++)
        free += !t->v[i].active;
    return free >= count && voices_busy() + count <= NVOICE;
}
static void ncd_overlap_begin(uint32_t part)
{
    uint32_t i;
    ncd_overlapping[part] = 1;
    for (i = 0; i < NVOICE; i++)
        if (trk[part].v[i].active && ncd_overlap[part][i].phase != NCD_FADE_OUT) {
            ncd_overlap[part][i].from = (uint16_t)ncd_overlap_gain(part, i);
            ncd_overlap[part][i].gain = ncd_overlap[part][i].from;
            ncd_overlap[part][i].pos = 0;
            ncd_overlap[part][i].phase = NCD_FADE_OUT;
        }                                           /* already retiring: keep its original deadline */
}
static void ncd_overlap_note(track_t *t, voice_t *v)
{
    uint32_t part = (uint32_t)(t - trk), i = (uint32_t)(v - t->v);
    if (part >= NCD_FADE_PARTS)
        return;
    ncd_overlap[part][i].pos = ncd_overlap[part][i].from = ncd_overlap[part][i].gain = 0;
    ncd_overlap[part][i].phase = ncd_overlapping[part] ? NCD_FADE_IN : NCD_FADE_IDLE;
}
static void ncd_overlap_tick(uint32_t part)
{
    uint32_t i, any = 0;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[part].v[i];
        uint32_t x, curve, length;
        if (!v->active)
            ncd_overlap[part][i].phase = NCD_FADE_IDLE;
        if (!ncd_overlap[part][i].phase)
            continue;
        length = ncd_overlap[part][i].phase == NCD_FADE_OUT ? NCD_FADE_BLOCKS : NCD_RISE_BLOCKS;
        if (ncd_overlap[part][i].pos == length) {      /* zero endpoint has already been rendered */
            if (ncd_overlap[part][i].phase == NCD_FADE_OUT) {
                v->active = v->gate = v->stage = 0;
                v->env = v->env_out = 0;
            }
            ncd_overlap[part][i].phase = NCD_FADE_IDLE;
            continue;
        }
        x = (uint32_t)++ncd_overlap[part][i].pos * NCD_FADE_FULL / length;
        curve = (((x * x) >> 15) * (3u * NCD_FADE_FULL - 2u * x)) >> 15;
        ncd_overlap[part][i].gain = (uint16_t)(ncd_overlap[part][i].phase == NCD_FADE_OUT ?
            (ncd_overlap[part][i].from * (NCD_FADE_FULL - curve)) >> 15 : curve);
        any = 1;
    }
    ncd_overlapping[part] = (uint8_t)any;
}
static int32_t ncd_overlap_amp(uint32_t part, uint32_t i, int32_t amp)
{
    if (part < NCD_FADE_PARTS && ncd_overlap[part][i].phase)
        return (int32_t)((amp * (int32_t)ncd_overlap_gain(part, i)) >> 15);
    return amp;
}
