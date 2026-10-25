/* SPDX-License-Identifier: GPL-3.0-only */
/* NC: REPLACE's musical fade, shared by all engines without extra voices or DSP/sample buffers. The event
 * core alone advances these gains before handing parts to either render core; renderers only read them.
 * A smoothstep ramp takes 344 blocks (~250 ms) each way when overlap cannot fit. Engine amplitude ramps interpolate each block's
 * endpoints per sample. Phase 0 is unity (including zero-initialized boot); ordinary notes are unchanged. */
#define NCD_FADE_PARTS 2u
#define NCD_FADE_BLOCKS 344u
#define NCD_FADE_FULL 32768u
enum { NCD_FADE_IDLE, NCD_FADE_OUT, NCD_FADE_IN };
static struct {
    uint16_t gain, from;
    uint16_t pos;
    uint8_t phase;
} ncd_fade[NCD_FADE_PARTS];

static uint32_t ncd_fade_gain(uint32_t part)
{
    return ncd_fade[part].phase ? ncd_fade[part].gain : NCD_FADE_FULL;
}
static void ncd_fade_tick(uint32_t part)
{
    uint32_t x, curve;
    if (!ncd_fade[part].phase || ncd_fade[part].pos == NCD_FADE_BLOCKS)
        return;
    x = (uint32_t)++ncd_fade[part].pos * NCD_FADE_FULL / NCD_FADE_BLOCKS;
    curve = (((x * x) >> 15) * (3u * NCD_FADE_FULL - 2u * x)) >> 15;
    if (ncd_fade[part].phase == NCD_FADE_OUT)
        ncd_fade[part].gain = (uint16_t)((ncd_fade[part].from * (NCD_FADE_FULL - curve)) >> 15);
    else {
        ncd_fade[part].gain = (uint16_t)(ncd_fade[part].from +
            (((NCD_FADE_FULL - ncd_fade[part].from) * curve) >> 15));
        if (ncd_fade[part].pos == NCD_FADE_BLOCKS)
            ncd_fade[part].phase = NCD_FADE_IDLE;
    }
}
static uint32_t ncd_fade_out(uint32_t part)
{
    if (ncd_fade[part].phase != NCD_FADE_OUT) {
        ncd_fade[part].from = (uint16_t)ncd_fade_gain(part);
        ncd_fade[part].gain = ncd_fade[part].from;
        ncd_fade[part].phase = NCD_FADE_OUT;
        ncd_fade[part].pos = 0;
    }
    return NCD_FADE_BLOCKS - ncd_fade[part].pos; /* fast strikes replace the queue, never restart the countdown */
}
static void ncd_fade_in(uint32_t part)
{
    ncd_fade[part].from = (uint16_t)ncd_fade_gain(part);
    ncd_fade[part].phase = NCD_FADE_IN;
    ncd_fade[part].pos = 0;
}
static int32_t ncd_fade_amp(uint32_t part, int32_t amp)
{
    if (part < NCD_FADE_PARTS && ncd_fade[part].phase)
        return (int32_t)((amp * (int32_t)ncd_fade_gain(part)) >> 15);
    return amp;
}
