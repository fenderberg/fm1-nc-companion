/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the Juno chorus and the split output, the mix of an NCC part
 * (tracks 1 and 2) in place of mix_part's level / pan / sends loop when either is on (fx.c, ncc_custom).
 *
 * The Juno character of a part is P_ED_FX (a stored parameter nothing else reads, so it is the sound's and comes with
 * projects and user presets; a sound load puts it back to 0): bits 0..1 the chorus mode, bits 2..5 the drift (voice.c
 * poly_play: each POLY note's fine tune a random 0 .. +-7.5 cents at 15, as a Juno's voices are never quite in tune).
 *
 * JUNO CHORUS (per part). The part's mono block after DIST /
 * INSERT / SLICER goes into a 256-sample line (5.8 ms); two taps read it at a triangle-swept delay, 180 degrees apart:
 * left = dry + tap A, right = dry + tap B, as the Juno-60's bucket-brigade pair. A one-pole low-pass on each tap
 * (~7 kHz: the BBD's dullness); the wet at -3 dB against the dry. The modes (after analyses of the Juno-60's circuit):
 *   I     0.51 Hz, 1.66 .. 5.35 ms
 *   II    0.86 Hz, 1.66 .. 5.35 ms
 *   I+II  9.75 Hz, 3.3 .. 3.7 ms (fast and shallow: a vibrato-like shimmer)
 * A mode change fades the wet out (2.9 ms, SL_SLOPE), clears the line, fades the new one in: no click. OFF and faded:
 * mix_part's own loop again, bit for bit as without NCC.
 *
 * SPLIT, NCC's default (G_ENGGO = 1 is STEREO: a stored global nothing reads, "unused, ids fixed by the formats", 0 in
 * every project and at power-on, so a project plays split until GLO + SEQ turns it to STEREO): part L entirely on the left
 * output, part R on the right, each mono (PAN ignored), for the Circuit Tracks' two audio inputs: the dry sound only.
 * The delay, reverb and chorus buses are shared, so their return (wet, HALL's side, throws, tails) comes out on both
 * sides as in STEREO (Remco's choice: the effects heard, the dry parts apart); with every send at 0 the sides are
 * exactly apart. Not the small speaker's BASS+ of both sides (fx.c master_out). Its Juno chorus is summed onto its own
 * side. DIST, INSERT, the SLICER and PERFORM's effects (each side its own) keep working.
 *
 * State in the pool (zeroed at boot), per part; the audio ISR only (the main loop writes P_ED_FX and G_ENGGO). */

#define JC_LEN 256u                     /* the line, samples (a power of 2: masked) */
#define JC_LPK 19661                    /* the taps' one-pole low-pass, Q15 (~7 kHz at 44.1 kHz) */
#define JC_WET 23170                    /* the wet against the dry, Q15 (-3 dB) */
enum { JC_OFF, JC_I, JC_II, JC_I_II, JC_N };
static const uint32_t JC_INC[JC_N] = {0, 49962u, 84049u, 949567u};   /* the LFO per sample: Hz x 2^32 / 44100 */
static const uint16_t JC_LO[JC_N] = {0, 18739u, 18739u, 37255u};     /* the delay's ends, Q8 samples */
static const uint16_t JC_HI[JC_N] = {0, 60394u, 60394u, 41775u};
static const char *const JC_NAME[JC_N] = {"OFF", "I", "II", "I+II"};
typedef struct {
    int16_t line[JC_LEN];               /* the part's block, at a quarter (headroom) */
    uint16_t wp;                        /* the line's next write */
    uint8_t mode;                       /* the mode running */
    int32_t w;                          /* the wet share now, Q15 */
    uint32_t ph;                        /* the LFO's phase */
    int32_t la, lb;                     /* the taps' low-passes */
} jc_t;
static jc_t jc[NCM_PARTS] __attribute__((section(".pool")));

static inline uint32_t jc_mode(const track_t *t) { return (uint32_t)clamp(t->p[P_ED_FX], 0, 63) & 3u; }

/* mix_part: track t's mix is NCC's (a Juno chorus on or fading, or SPLIT) */
static inline int ncc_custom(const track_t *t)
{
    uint32_t part = (uint32_t)(t - trk);
    return part < NCM_PARTS && (NCC_SPLIT() || jc_mode(t) || jc[part].w);
}

/* one tap: the line read d (Q8 samples) behind the write, through its low-pass lp */
static inline int32_t jc_tap(const jc_t *s, uint32_t d, int32_t *lp)
{
    int32_t r0 = (int32_t)(s->wp - (d >> 8)) & (int32_t)(JC_LEN - 1u), r1 = (r0 - 1) & (int32_t)(JC_LEN - 1u);
    int32_t v = s->line[r0] + (((s->line[r1] - s->line[r0]) * (int32_t)(d & 255u)) >> 8);
    *lp += (((v << 2) - *lp) >> 2) * JC_LPK >> 13;       /* ((x - lp) / 4) * k / 2^13 = (x - lp) * k / 2^15 */
    return *lp;
}

/* track t's level / pan / sends with its Juno chorus and SPLIT (mix_part's loop, NCC's way) */
static __attribute__((noinline)) void ncc_mix(track_t *t, int32_t *b, uint32_t n SM_DECL)
{
    uint32_t part = (uint32_t)(t - trk), i, m = jc_mode(t), split = NCC_SPLIT(), lo, span, inc;
    jc_t *s = &jc[part];
    int32_t lvl = LEVEL_Q12[t->p[P_LEVEL] & 127], pan = t->p[P_PAN];
    int32_t gl = 4096 - (pan > 0 ? pan * 64 : 0), gr = 4096 + (pan < 0 ? pan * 64 : 0);
    int32_t c = t->p[P_CHOR] * 258, d = t->p[P_DLY] * 258, r = t->p[P_REV] * 258, pk = t->peak, xmax, tw;
    if (s->mode != m && !s->w) {                        /* faded out: the new mode from an empty line */
        memset(s, 0, sizeof *s);
        s->mode = (uint8_t)m;
    }
    tw = s->mode == m && m ? 32768 : 0;
    lo = JC_LO[s->mode];
    span = (uint32_t)(JC_HI[s->mode] - JC_LO[s->mode]);
    inc = JC_INC[s->mode];
    if (split) {                                        /* L left, R right (the sends as ever: their return on */
        gl = part ? 0 : 4096;                           /* both sides) */
        gr = part ? 4096 : 0;
    }
    xmax = c > d ? c : d;
    xmax = 0x7FFFFFFF / ((xmax > r ? xmax : r) | 1);    /* sends: loud chords at a high LEVEL (as mix_part) */
    for (i = 0; i < n; i++) {
        int32_t x = ((b[i] >> 2) * lvl) >> 10, a = x < 0 ? -x : x, xs = clamp(x, -xmax, xmax), wa = 0, wb = 0;
        if (a > pk)
            pk = a;
        if (c)
            SM(c)[i] += mulq15(xs, c);
        if (d)
            SM(d)[i] += mulq15(xs, d);
        if (r)
            SM(v)[i] += mulq15(xs, r);
        if (s->w | tw) {
            uint32_t g;
            s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
            wa = jc_tap(s, lo + ((span * pf_tri(s->ph)) >> 16), &s->la);
            wb = jc_tap(s, lo + ((span * pf_tri(s->ph + 0x80000000u)) >> 16), &s->lb);
            s->line[s->wp] = (int16_t)clamp(x >> 2, -32767, 32767);
            s->wp = (uint16_t)((s->wp + 1u) & (JC_LEN - 1u));
            s->ph += inc;
            g = (uint32_t)((s->w * JC_WET) >> 15) << 1;   /* Q16 */
            wa = mulq16(wa, g);
            wb = mulq16(wb, g);
        }
        if (split) {                                    /* mono on its own side: the two taps halved */
            int32_t y = (x >> 1) + ((wa + wb) >> 2);    /* (halved before the gain: dry + wet stays in 32 bits) */
            SM(l)[i] += (y * gl) >> 11;
            SM(r)[i] += (y * gr) >> 11;
        } else {
            SM(l)[i] += (((x + wa) >> 1) * gl) >> 11;
            SM(r)[i] += (((x + wb) >> 1) * gr) >> 11;
        }
    }
    t->peak = pk;
}
