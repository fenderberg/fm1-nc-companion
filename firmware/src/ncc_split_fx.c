/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the effect buses, SPLIT's a side each. In SPLIT (NCC's default, core.h NCC_SPLIT) part L goes
 * to the left output only and part R to the right only, for the Circuit Tracks' two audio inputs: the dry sound
 * (juno_chorus.c ncc_mix) and, here, its chorus, delay and reverb too (Remco, 2026-11-15: "altijd volledig
 * gescheiden"). One set of settings (the song's G_C* G_D* G_R*: the same knobs as ever), two of each bus: L's sends
 * (send_c / send_d / send_r) into the left ones, R's (ncs_snd) into the right ones, each side's return on its own
 * output. The throws (perf_throw: ENV / LFO held) follow the part; PERFORM's THROW macro takes the left mix into the
 * left buses and the right into the right (ncs_pre). The master's limiter is off in SPLIT (fx.c master_out: a loud L
 * no longer ducks R; the soft clip stays, a side each; the Circuit has its own).
 *
 * One code for both layouts (fx.c's fx_buses, rev_switch and the three rev_* are left unused in NCC, so not built:
 * the flash holds no two of them). STEREO is one side at Felucca's lengths, bit for bit fx_buses (the regression's
 * golden renders); SPLIT is two sides in the halves of the same buffers (no RAM of their own):
 *   CHORUS  cho_buf as two lines of NCS_CHO (1024) samples: the sweep from NCS_CHO_D (5.8 ms) instead of 9 ms, as
 *           deep as ever (DEPTH 127: + 17.3 ms), so its longest read (1019) fits.
 *   DELAY   dly_buf as two lines of NCS_DLY (0.74 s) (Remco's choice A: the sound as it is, half the time). A TIME
 *           longer than that (1/2 below 162 BPM, 1/4D below 122, 1/4 below 81) plays at half its length (and half again
 *           where that is still too long): on the beat still.
 *   REVERB  rev_comb / rev_u as two of each model at about half their lengths (Remco: "dunner maar gescheiden" if
 *           need be). ROOM: 4 combs and 2 allpasses of prime lengths about half ROOM's; the combs' gain the square root
 *           of ROOM's (two passes of the half lengths decay as one of the whole) and their low-pass the one whose two
 *           passes damp the top as ROOM's one does: the same decay, half the echo density (heard: a little more
 *           metallic). SPRING: the loop up to 46 ms (60: SIZE's top shorter, from about 70). HALL: 8 lines of half the
 *           lengths (prime, 10.6 .. 16.9 ms at 22.05 kHz), their gains from their lengths (hall_gains' RT60: the
 *           same tails), a mono mid per side (the stereo of HALL needs both sides). Each side costs what the model
 *           cost: twice the reverb's work in SPLIT.
 * A switch between SPLIT and STEREO: the old layout's last block fades out (as rev_switch), the chorus and reverb
 * buffers are cleared at once, the delay line in NCS_CLR blocks (23 ms, 4 KB a block: no long clear in one ISR);
 * meanwhile no bus plays (their sends are dropped), then the new layout starts from silence.
 *
 * The audio ISR only (the main loop writes G_ENGGO and the settings). ncs.split is the block's routing (ncs_begin, at
 * its start: the second core reads it while it renders); ncs.run the layout the buffers hold. */
#define NCS_CHO (CHO_LEN / 2u)                  /* a side's chorus line */
#define NCS_CHO_D 256                           /* its sweep's short end, samples (STEREO: 400) */
#define NCS_DLY (DLY_LEN / 2u)                  /* a side's delay line */
#define NCS_CLR 32u                             /* blocks the delay line's clear takes */
#define NCS_HALF (sizeof rev_comb / 2u / 2u)    /* a side's share of rev_comb (2468 samples) */
#define NCS_APH (sizeof rev_u.ap / 2u / 2u)     /* .. of rev_u's allpasses (498) */
#define NCS_SPAP (4u * (SP_N + 1u))             /* .. SPRING's allpass states (int32) */
#define NCS_SPL 2048u                           /* SPRING's loop line (a power of 2, as SP_LEN: masked) */
static const uint16_t NCS_COMB[4] = {557, 593, 631, 677};   /* ROOM's, about halved (primes) */
static const uint16_t NCS_AP[2] = {277, 211};
static const uint16_t NCS_HLEN[HL_NL] = {233, 251, 271, 293, 313, 337, 353, 373};   /* HALL's (+1 each: the mirror) */
static const uint16_t NCS_HAP[HL_NAP] = {67, 151};
static const uint8_t NCS_QR[HL_NL][3] = {{0, 0, 0}, {2, 3, 5}, {3, 4, 6}, {4, 5, 7}, {5, 6, 1}, {6, 7, 2}, {7, 1, 3},
                                         {1, 2, 4}};   /* HALL's matrix (rev_hall): row k's lines at +1 (p_k-1) */
_Static_assert(DLY_LEN % NCS_CLR == 0u && NCS_CHO_D + 127 * 6 + 1 < (int)NCS_CHO, "SPLIT: the delay's clear, the chorus' line");
_Static_assert(557 + 593 + 631 + 677 <= NCS_HALF && 277 + 211 <= NCS_APH, "SPLIT: ROOM in half the buffers");
_Static_assert(NCS_SPL <= NCS_HALF && !(NCS_SPL & (NCS_SPL - 1u)) && 2u * NCS_SPAP <= sizeof rev_u.sp / 4u,
               "SPLIT: SPRING");
_Static_assert(233 + 251 + 271 + 293 + 313 + 337 + 353 + 373 + HL_NL <= NCS_HALF && 67 + 151 <= NCS_APH &&
               HL_NC + 2u * HL_MOD + 2u < 233u && HL_NC <= 67u, "SPLIT: HALL in half the buffers");
_Static_assert(2645u + 8u <= SP_LEN, "STEREO: SPRING's line as rev_spring's (its longest loop never capped)");

typedef struct {                                /* a side's reverb (each model's own states) */
    uint16_t ci[4], ai[2];                      /* ROOM: the combs' and allpasses' indices */
    int32_t clp[4];                             /* .. the combs' low-passes */
    uint16_t sw;                                /* SPRING: the loop's write index (masked by its line's length) */
    int32_t slp, shp, she, ssz;                 /* .. its low-pass, low cut (and its remainder), length (Q8) */
    uint16_t hi[HL_NL], hd[HL_NAP];             /* HALL: the lines' and diffusers' indices */
    int32_t hlp[HL_NL], hle[HL_NL];             /* .. the low-passes and their remainders */
    int32_t hx, hm, hs, hhp, hhe;               /* .. the send's last sample, the last mid and side out, low cut */
} ncs_side_t;
typedef struct {                                /* a layout's reverb: where each model of a side lives */
    const uint16_t *comb, *ap, *hlen, *hap;     /* ROOM's and HALL's lengths */
    uint32_t half, aph, spl;                    /* a side's share of rev_comb, of rev_u.ap; SPRING's line */
} ncs_lay_t;
static const ncs_lay_t NCS_LAY[2] = {
    {REV_COMB, REV_AP, HL_LEN, HL_AP, 0u, 0u, SP_LEN},          /* STEREO: Felucca's, one side */
    {NCS_COMB, NCS_AP, NCS_HLEN, NCS_HAP, NCS_HALF, NCS_APH, NCS_SPL},
};
static struct {
    ncs_side_t s[2];
    int32_t dlp[2];                             /* the delays' feedback low-passes */
    int16_t hg[HL_NL], hsz;                     /* HALL's gains, for SIZE and layout hsz - 1 (0: none yet) */
    int32_t rsz, rkp;                           /* ROOM's comb gain and low-pass (Q15) */
    uint32_t hph, sph;                          /* HALL's drift, SPRING's wobble */
    uint8_t split;                              /* this block's routing: 1 SPLIT (ncs_begin) */
    uint8_t run;                                /* the layout the buffers hold: 1 SPLIT's */
    uint8_t clr;                                /* blocks of the delay line's clear left */
} ncs __attribute__((section(".pool")));
/* R's sends in SPLIT (chorus, delay, reverb). One for both cores: R renders on one of them a block (mix_part on it,
 * its sends and throws with it), this core reads them after the second core is done (mix_block: C1_WAIT), as mx[1].
 * (A second core that stops answering mid-block has its parts rendered again here: R's sends of that one block
 * twice, a little more of the buses once; mx[1] is cleared for that, these are not, to keep the code small) */
static int32_t ncs_snd[3][CTL];
static int32_t ncs_w[2][CTL];                   /* the sides' returns */
/* track t's sends are R's own (SPLIT) (fx.c ncc_mix, ncs_throw) */
#define NCS_R(t) (ncs.split && (t) == &trk[1])

/* REV / ECHO THROW of track t (fx.c mix_part: perf_throw into the mix's sends sd sr; SPLIT: R's into its own) */
static __attribute__((noinline)) void ncs_throw(const track_t *t, const int32_t *b, int32_t *sd, int32_t *sr,
                                                uint32_t n)
{
    if (NCS_R(t))
        sd = ncs_snd[1], sr = ncs_snd[2];
    perf_throw(t, b, sd, sr, n);
}

/* the block's routing, before the parts render (fx.c mix_block) */
static inline void ncs_begin(void) { lim_off = ncs.split = (uint8_t)NCC_SPLIT(); }

static uint32_t ncs_isqrt(uint32_t x)           /* floor(sqrt(x)) */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* the delay's TIME in samples (delay_samples'): STEREO at most DLY_LEN - 1 (delay_samples' clamp); SPLIT halved until
 * it fits a side's line, from its exact length (so it stays on the beat) */
static uint32_t ncs_delay_samples(uint32_t split)
{
    uint32_t d = (uint32_t)song.g[G_DTIME];
    uint32_t s = d - 10u < 3u ? (beat_samples() * 3u) >> DIV_DOT[d - 10u] : div_samples(d);
    if (!split)
        s = s >= DLY_LEN ? DLY_LEN - 1u : s;
    while (s >= NCS_DLY && split)
        s >>= 1;
    return s < 16u ? 16u : s;
}

typedef struct {                                /* the chorus and delay's values for a block (fx_buses') */
    int16_t *cb, *db;                           /* a side's lines */
    uint32_t cm, dm, dl, cinc;                  /* their masks, the delay in samples, the sweep's step */
    int32_t cd, fb, col, dmix, cdepth;          /* the sweep's short end (Q8), feedback, colour, level, depth */
} ncs_cd_t;

/* a side's chorus and delay (fx_buses' loop: cin din its sends, *lp its delay's low-pass) into w; the lines' indices
 * and the sweep from fx's, which the caller moves on after the last side */
static void ncs_chodly(const ncs_cd_t *p, const int32_t *cin, const int32_t *din, int32_t *lp,
                                                 int32_t *w, uint32_t n)
{
    uint32_t i, cw = fx.cho_w, dw = fx.dly_w, ph = fx.cho_ph;
    int16_t *cb = p->cb, *db = p->db;
    const uint32_t cm = p->cm, dm = p->dm, dl = p->dl, cinc = p->cinc;      /* (in registers: the loop's stores are */
    const int32_t cd = p->cd, fb = p->fb, col = p->col, dmix = p->dmix, cdepth = p->cdepth;   /* int16) */
    int32_t l = *lp;
    for (i = 0; i < n; i++, cw++, dw++) {
        int32_t y, x, r, f, c0, c1;
        uint32_t ri;
        cb[cw & cm] = (int16_t)clamp(cin[i] >> 1, -32768, 32767);
        ph += cinc;
        r = cd + ((osc_sine(ph) + 32768) * cdepth >> 8);
        ri = (uint32_t)r >> 8;
        f = r & 255;
        c0 = cb[(cw - ri) & cm];
        c1 = cb[(cw - ri - 1u) & cm];
        y = (c0 + (((c1 - c0) * f) >> 8)) << 1;
        x = db[(dw - dl) & dm];
        l += mulq15(x - l, col);
        db[dw & dm] = (int16_t)clamp((din[i] >> 1) + mulq15(l, fb), -32768, 32767);
        w[i] = y + mulq15(x << 1, dmix);
    }
    *lp = l;
}

/* ROOM, a side (rev_room: c its combs, a its allpasses, of the lengths cl al), added to out. Its states in locals for
 * the block (v's uint16 indices could be any of the lines' int16 stores to the compiler: reloaded every sample) */
static void ncs_room(ncs_side_t *v, int16_t *c0, int16_t *a0, const uint16_t *cl,
                                               const uint16_t *al, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, ci[4], ai[2], ln[4], an[2];
    int32_t size = ncs.rsz, keep = ncs.rkp, lp[4];
    for (k = 0; k < 4u; k++)
        ci[k] = v->ci[k], ln[k] = cl[k], lp[k] = v->clp[k];
    ai[0] = v->ai[0], ai[1] = v->ai[1], an[0] = al[0], an[1] = al[1];
    for (i = 0; i < n; i++) {
        int32_t a = 0, in = mulq15(rev_in[i], 2580);
        int16_t *c = c0;
        for (k = 0; k < 4u; k++) {
            int32_t o = c[ci[k]];
            lp[k] = o + mulq15(lp[k] - o, keep);
            c[ci[k]] = (int16_t)clamp(in + mulq15(lp[k], size), -32768, 32767);
            if (++ci[k] >= ln[k])
                ci[k] = 0;
            a += o;
            c += ln[k];
        }
        c = a0;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[ai[k]], w = a + (o >> 1);
            c[ai[k]] = (int16_t)clamp(w, -32768, 32767);
            a = o - a;
            if (++ai[k] >= an[k])
                ai[k] = 0;
            c += an[k];
        }
        out[i] += a;
    }
    for (k = 0; k < 4u; k++)
        v->ci[k] = (uint16_t)ci[k], v->clp[k] = lp[k];
    v->ai[0] = (uint16_t)ai[0], v->ai[1] = (uint16_t)ai[1];
}

/* ROOM's comb gain and low-pass (once a block). STEREO rev_room's; SPLIT's half lengths: the gain sqrt(g) (two
 * passes, one decay), the low-pass's pole b with ((1 - b) / (1 + b))^2 = (1 - a) / (1 + a) (its two passes at fs / 2
 * as ROOM's one) */
static void ncs_room_k(uint32_t split)
{
    uint32_t g = 25000u + (uint32_t)song.g[G_RSIZE] * 50u, a = (uint32_t)song.g[G_RDAMP] * 200u, r;
    if (!split) {
        ncs.rsz = (int32_t)g;
        ncs.rkp = (int32_t)a;                                               /* (rev_room's 32767 - damp) */
        return;
    }
    ncs.rsz = (int32_t)ncs_isqrt(g << 15);                                  /* (g < 2^15: g << 15 < 2^30) */
    r = ncs_isqrt((((32768u - a) << 15) / (32768u + a)) << 15);             /* (a <= 25400: no 0 divisor) */
    ncs.rkp = (int32_t)(((32768u - r) << 15) / (32768u + r));
}

/* SPRING, a side (rev_spring in a line of sl, a power of 2: ln; ap its allpasses' states), added to out (its states in
 * locals for the block, as ROOM's) */
static void ncs_spring(ncs_side_t *v, int16_t *ln, uint32_t sl, int32_t *ap,
                                                 const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)song.g[G_RSIZE], L, L2, L3, wp = v->sw, m = sl - 1u;
    int32_t g = 19661 + (int32_t)s * 85, kl = 26000 - song.g[G_RDAMP] * 160, f, w;
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, slp = v->slp, shp = v->shp, she = v->she;
    if (len > (int32_t)(sl - 8u) << 8)
        len = (int32_t)(sl - 8u) << 8;                                      /* (SPLIT's line: 46 ms) */
    if (!v->ssz)
        v->ssz = len;
    v->ssz += clamp(len - v->ssz, -256, 256);
    if (v->ssz > len && v->ssz > (int32_t)(sl - 8u) << 8)
        v->ssz = (int32_t)(sl - 8u) << 8;                                   /* (never past the line) */
    L = (uint32_t)v->ssz >> 8;
    w = (v->ssz >> 1) + ((osc_sine(ncs.sph) * 3) >> 8);
    L2 = (uint32_t)w >> 8;
    f = w & 255;
    L3 = (L * 3u) >> 2;
    for (i = 0; i < n; i++) {
        uint32_t j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - L) & m], p, o;
        int32_t t0 = ln[(wp - L2) & m], t1 = ln[(wp - L2 - 1u) & m];
        slp += mulq15(r - slp, kl);
        o = slp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;
        o = x - shp + she;
        she = o & 63;
        shp += o >> 6;
        x -= shp;
        p = ap[j];
        ap[j] = x;
        for (k = 1; k <= SP_N; k++) {
            int32_t q = (x - ap[j + k]) * SP_A;
            o = ap[j + k];
            x = ((q + ((q >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & m] = (int16_t)clamp(x, -32768, 32767);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - L3) & m] * 2;
        wp++;
    }
    v->sw = (uint16_t)wp, v->slp = slp, v->shp = shp, v->she = she;
}

/* HALL's gains (hall_gains) for the lines of len, SIZE s */
static void ncs_hall_gains(const uint16_t *len, int32_t s, int32_t key)
{
    uint32_t q = exp2_neg((uint32_t)s * 65536u / 30u), k, g;
    for (k = 0; k < HL_NL; k++) {
        g = exp2_neg((len[k] * 59u * q) >> 15);
        ncs.hg[k] = (int16_t)(((g > 32440u ? 32440u : g) * 6193u) >> 15);
    }
    ncs.hsz = (int16_t)key;
}

/* HALL, a side (rev_hall: b its lines of len, a its diffusers of al; hl's block arrays as scratch): the mid added to
 * out, the side into sd (rev_side: STEREO's stereo; SPLIT's unused, a side is mono: hl.side 0) */
static void ncs_hall(ncs_side_t *v, int16_t *b0, int16_t *a, const uint16_t *len,
                                               const uint16_t *al, const int32_t *rev_in, int32_t *out, int32_t *sd)
{
    int16_t *b = b0;
    uint32_t i, k, s, m, d;
    int32_t kd = HL_KD(song.g[G_RDAMP]);
    int32_t xp = v->hx, hp = v->hhp, he = v->hhe, mp = v->hm, sp = v->hs;
    for (i = 0; i < HL_NC; i++) {
        int32_t x = (xp + 2 * rev_in[2u * i] + rev_in[2u * i + 1u]) >> 2, w;
        xp = rev_in[2u * i + 1u];
        x = mulq15(clamp(x, -262143, 262143), 2580);
        w = x - hp + he;
        he = w & 31;
        hp += w >> 5;
        hl.bx[i] = x - hp;
    }
    v->hx = xp, v->hhp = hp, v->hhe = he;
    for (k = 0; k < HL_NAP; k++) {
        d = v->hd[k];
        HL_RUNS(d, al[k], m, s, hall_ap(a + d, HL_APG[k], hl.bx + s, m));
        v->hd[k] = (uint16_t)d;
        a += al[k];
    }
    for (k = 0; k < HL_NL; k++) {
        int32_t g = ncs.hg[k], f = 0, lo;
        if ((HL_LPM >> k) & 1u)
            d = v->hi[k];
        else {
            uint32_t r = (uint32_t)(((HL_MOD + 2) << 8) +
                                    ((osc_sine(ncs.hph * (1u + (k & 1u)) + k * 0x2C000000u) * HL_MOD) >> 7));
            f = (int32_t)(r & 255u);
            d = v->hi[k] + (r >> 8);
            d = d >= len[k] ? d - len[k] : d;
        }
        s = d + len[k] / 2u;
        s = s >= len[k] ? s - len[k] : s;
        lo = (b[d] < 0 ? -b[d] : b[d]) + (b[s] < 0 ? -b[s] : b[s]) < HL_LOUD;
        if ((HL_LPM >> k) & 1u) {
            HL_RUNS(d, len[k], m, s, hall_end_lp(b + d, hl.y[k] + s, m, g, lo, &v->hlp[k], &v->hle[k], kd));
        } else {
            HL_RUNS(d, len[k], m, s, hall_end(b + d, f, hl.y[k] + s, m, g, lo));
        }
        b += len[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {                       /* (rev_hall's outputs and matrix, its rows in a loop: */
        int32_t y[HL_NL], t = 0, x = hl.bx[i];          /* the same sums, smaller code) */
        for (k = 0; k < HL_NL; k++)
            y[k] = hl.y[k][i], t += k ? y[k] : 0;
        hl.l[i] = y[0] + y[3] + y[4] - y[7];
        hl.r[i] = y[1] + y[2] + y[5] + y[6];
        y[0] += t;
        hl.y[0][i] = (int16_t)clamp(t + x, -32768, 32767);
        for (k = 1; k < HL_NL; k++)                     /* row k: 2 (the three at +1, 2, 4 after it) + y_k - y0 +- x */
            hl.y[k][i] = (int16_t)clamp(2 * (y[NCS_QR[k][0]] + y[NCS_QR[k][1]] + y[NCS_QR[k][2]]) + y[k] - y[0] +
                                        (k == 7u ? -x : x), -32768, 32767);
    }
    b = b0;
    for (k = 0; k < HL_NL; k++) {
        d = v->hi[k];
        HL_RUNS(d, len[k], m, s, for (i = 0; i < m; i++) b[d + i] = hl.y[k][s + i]; if (!d) b[len[k]] = b[0]);
        v->hi[k] = (uint16_t)d;
        b += len[k] + 1u;
    }
    for (i = 0; i < HL_NC; i++) {                       /* back to 44.1 kHz, between samples */
        int32_t mm = (hl.l[i] + hl.r[i]) * HL_OUT, ss = (hl.l[i] - hl.r[i]) * HL_OUT;
        out[2u * i] += (mp + mm) >> 1;
        out[2u * i + 1u] += mm;
        sd[2u * i] = (sp + ss) >> 1;
        sd[2u * i + 1u] = ss;
        mp = mm;
        sp = ss;
    }
    v->hm = mp, v->hs = sp;
}

/* model rt's per-block values for layout split (before its sides) */
static void ncs_rev_k(uint32_t rt, uint32_t split)
{
    if (rt == 2u) {
        int32_t key = (song.g[G_RSIZE] + 1) | (int32_t)split << 8;
        ncs.hph += HL_LFO;
        if (ncs.hsz != key)
            ncs_hall_gains(NCS_LAY[split].hlen, song.g[G_RSIZE], key);
    } else if (rt)
        ncs.sph += 2u * LFO_INC[24];
    else
        ncs_room_k(split);
}
/* model rt of layout split on side s (in its send), added to out */
static void ncs_rev(uint32_t rt, uint32_t split, uint32_t s, const int32_t *in, int32_t *out, uint32_t n)
{
    const ncs_lay_t *y = &NCS_LAY[split];
    int16_t *b = rev_comb + s * y->half, *a = rev_u.ap + s * y->aph;
    if (rt == 2u) {
        ncs_hall(&ncs.s[s], b, a, y->hlen, y->hap, in, out, rev_side);
        hl.side = !split;                               /* (STEREO: HALL's side to the mix, fx.c rev_side_mix) */
    }
    else if (rt)
        ncs_spring(&ncs.s[s], b, y->spl, rev_u.sp + s * NCS_SPAP, in, out, n);
    else
        ncs_room(&ncs.s[s], b, a, y->comb, y->ap, in, out, n);
}

/* dst = (add ? dst : 0) + src, src ramped from 1 down by dg (Q16) a sample (dg 0: as it is: mulq16 by 65536 is) */
static __attribute__((noinline)) void ncs_fade(int32_t *dst, const int32_t *src, int32_t dg, int add, uint32_t n)
{
    uint32_t i;
    int32_t g = 65536;
    for (i = 0; i < n; i++, g -= dg)
        dst[i] = (add ? dst[i] : 0) + mulq16(src[i], (uint32_t)g);
}

static void ncs_rev_reset(int keep)            /* the sides' reverb states (their buffers: rev_clear); keep: */
{                                               /* SPRING's length glides on (as rev_clear leaves fx.sp_size) */
    int32_t z0 = ncs.s[0].ssz, z1 = ncs.s[1].ssz;
    memset(ncs.s, 0, sizeof ncs.s);
    if (keep)
        ncs.s[0].ssz = z0, ncs.s[1].ssz = z1;
}

/* the buses of layout split for a block (STEREO: fx_buses; SPLIT: a side each): L's sends (sc sd sr), R's
 * (ncs_snd) -> w[0], w[1] (SPLIT); HALL's stereo side in STEREO into rev_side (hl.side) */
static void ncs_block(uint32_t split, const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *const *w,
                      uint32_t n)
{
    const int32_t *ins[2] = {sr, ncs_snd[2]};
    uint32_t s, rt = (uint32_t)song.g[G_RTYPE];
    ncs_cd_t p;
    p.fb = song.g[G_DFDBK] * 230;
    p.col = 2000 + song.g[G_DCOLOR] * 240;
    p.dmix = song.g[G_DMIX] * 258;
    p.cdepth = song.g[G_CDEPTH] * 6;
    p.cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    if (pf.ew1 && p.fb < THR_FB)
        p.fb += mulq16(THR_FB - p.fb, (uint32_t)pf.ew1 << 1);   /* ECHO THROW (as fx_buses) */
    p.cd = (split ? NCS_CHO_D : 400) << 8;
    p.cm = (split ? NCS_CHO : CHO_LEN) - 1u;
    p.dm = (split ? NCS_DLY : DLY_LEN) - 1u;
    p.dl = ncs_delay_samples(split);
    for (s = 0; s <= split; s++) {
        p.cb = cho_buf + s * NCS_CHO;
        p.db = dly_buf + s * NCS_DLY;
        ncs_chodly(&p, s ? ncs_snd[0] : sc, s ? ncs_snd[1] : sd, &ncs.dlp[s], w[s], n);
    }
    fx.cho_ph += p.cinc * n;
    fx.cho_w += n;
    fx.dly_w += n;
    rt = rt == 1u || rt == 2u ? rt : 0u;
    hl.side = 0;
    if (rt != fx.rtype) {                               /* the model changed: the old one's block fades out (as */
        int32_t *t = part_buf, dg = 65536 / (int32_t)n; /* rev_switch), the buffers cleared, the new from silence */
        ncs_rev_k(fx.rtype, split);
        for (s = 0; s <= split; s++) {
            memset(t, 0, n * sizeof t[0]);
            ncs_rev(fx.rtype, split, s, ins[s], t, n);
            ncs_fade(w[s], t, dg, 1, n);
        }
        if (hl.side)                                    /* (STEREO's HALL: its side faded too) */
            ncs_fade(rev_side, rev_side, dg, 0, n);
        rev_clear();
        ncs_rev_reset(1);
        fx.rtype = (uint8_t)rt;
    } else {
        ncs_rev_k(rt, split);
        for (s = 0; s <= split; s++)
            ncs_rev(rt, split, s, ins[s], w[s], n);
    }
}

/* the buses for a block (fx.c mix_block, in place of fx_buses): STEREO's return into wet (and rev_side), SPLIT's each
 * side into its own mix (ml, mr), wet 0; a change of layout as at the top */
static __attribute__((noinline)) void ncs_buses(int32_t *sc, int32_t *sd, int32_t *sr, int32_t *wet, int32_t *ml,
                                                int32_t *mr, uint32_t n)
{
    uint32_t i, run = ncs.run;
    int32_t *w[2] = {run ? ncs_w[0] : wet, ncs_w[1]};
    int32_t dg = run != ncs.split ? 65536 / (int32_t)n : 0;   /* (a change: the old layout faded out) */
    if (ncs.clr) {                                      /* the delay line cleared, a part a block; no bus plays */
        i = NCS_CLR - ncs.clr--;
        memset(dly_buf + i * (DLY_LEN / NCS_CLR), 0, DLY_LEN / NCS_CLR * sizeof dly_buf[0]);
        memset(wet, 0, n * sizeof wet[0]);
        hl.side = 0;
    } else {
        ncs_block(run, sc, sd, sr, w, n);
        if (run) {
            ncs_fade(ml, w[0], dg, 1, n);
            ncs_fade(mr, w[1], dg, 1, n);
            memset(wet, 0, n * sizeof wet[0]);
        } else if (dg) {
            ncs_fade(wet, wet, dg, 0, n);
            ncs_fade(rev_side, rev_side, dg, 0, n);
        }
    }
    if (dg) {                                           /* the buffers cleared for the new layout */
        rev_clear();
        memset(cho_buf, 0, sizeof cho_buf);
        ncs_rev_reset(0);                               /* (SPRING's length: the new line's) */
        ncs.dlp[0] = ncs.dlp[1] = 0;
        ncs.hsz = 0;
        ncs.clr = NCS_CLR;
        ncs.run = ncs.split;
    }
    if (ncs.split)
        memset(ncs_snd, 0, sizeof ncs_snd);
}

/* PERFORM's THROW (KNOB 3 with FX): STEREO perf_pre; SPLIT perf_pre a side each, the left mix into the left buses and
 * the right into the right (its glide once: both from the block's start; a side at the level a part gets in STEREO) */
static __attribute__((noinline)) void ncs_pre(const int32_t *ml, const int32_t *mr, int32_t *sd, int32_t *sr,
                                              uint32_t n)
{
    int32_t td = pf.td;
    if (!ncs.split) {
        perf_pre(ml, mr, sd, sr, n);
        return;
    }
    perf_pre(ml, ml, sd, sr, n);
    pf.td = td;
    perf_pre(mr, mr, ncs_snd[1], ncs_snd[2], n);
}
