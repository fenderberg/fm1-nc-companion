/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the live UI's screen (240 x 240), in the Circuit Tracks' manner: black, the parts in the
 * colours of the Circuit's tracks that play them (L: MIDI 1, blue; R: MIDI 2, pink), bright blocks, a white playhead.
 *   HEAD    the page (SOUND / ARP and its page 2; in the FX mode its page: PERFORM SENDS DELAY REVERB CHORUS INSERT)
 *           in a block of the focused part's colour,
 *           NCC, ST / SPLIT, the tempo (amber on an external clock)
 *   PARTS   L and R as two pads: the focused one lit in its colour, the other dark; channel, engine, the sound,
 *           ARP / HOLD / DRONE badges, SLOT (FM6) or the Juno chorus (ANALOG), a MIDI dot.
 *           the FX mode's PERFORM page: PERFORM's key map instead, the 16 white keys as pads (lit while they play)
 *   STEPS   the bar's 16 sixteenths as pads, the white playhead on the clock (seq.c beat_pos: ncc_bar)
 *   KNOBS   KNOB 1..4 of the page as rings (the Circuit's macro LEDs): the label, the arc in the part's colour (white
 *           just turned), the value
 *   FOOT    a popup (a Circuit knob: its legend and value; a button: what it did) for LV_POP_MS, else the page's hint
 * Each region is drawn on the canvas and blitted only when what it shows changed (lv.sig); a full screen only after
 * ui.force (the update countdown, an update). */

#define LC_BG RGB(0, 0, 0)
#define LC_PAD RGB(18, 18, 22)                          /* a dark pad */
#define LC_LINE RGB(44, 44, 52)
#define LC_DIM RGB(84, 84, 96)
#define LC_MID RGB(160, 160, 172)
#define LC_TEXT RGB(255, 255, 255)
#define LC_AMBER RGB(255, 168, 0)
static const uint16_t LC_PART[LV_PARTS] = {RGB(24, 128, 255), RGB(255, 64, 168)};    /* the Circuit's MIDI 1, MIDI 2 */
static const uint16_t LC_PDIM[LV_PARTS] = {RGB(6, 34, 72), RGB(74, 14, 48)};         /* .. at about a quarter */
enum { RG_HEAD, RG_PARTS, RG_STEPS, RG_KNOBS, RG_FOOT, RG_N };
#define LY_PARTS 28u
#define LH_PARTS 86u
#define LY_STEPS 116u
#define LY_KNOBS 132u
#define LH_KNOBS 80u
#define LY_FOOT 214u
/* PERFORM's effects on the pads (perform.c PF_*), two short lines each (a pad is 26 px wide) */
static const char *const LV_PF[PF_N + 1][2] = {
    {"REP", "1/8"}, {"REP", "1/16"}, {"REP", "1/32"}, {"REV", ""}, {"LPF", ""}, {"HPF", ""}, {"TAP", "STP"}, {"FRZ", ""},
    {"OCT", "UP"}, {"OCT", "DN"}, {"FLG", ""}, {"PHS", ""}, {"REV", "THR"}, {"ECH", "THR"},
    {"M1", ""}, {"M2", ""}, {"M3", ""}, {"M4", ""}, {"", ""}};

static uint32_t lv_hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static void lv_num(char *b, uint32_t v)                 /* v (< 1000) in decimal */
{
    char t[4];
    uint32_t n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v && n < 3u);
    while (n)
        *b++ = t[--n];
    *b = 0;
}
static int lv_ext(void) { return song.g[G_CLOCK] && midi_clock.have_pulse && fm1_ms - midi_clock.last_ms < 300u; }
static uint32_t lv_bpm(void)
{
    uint32_t b = beat_samples();
    return b ? ((uint32_t)FS * 60u + b / 2u) / b : 0u;
}
static int lv_changed(uint32_t r, uint32_t sig)         /* region r shows sig now: 1 = draw it */
{
    sig |= 1u;                                          /* (0 is "redraw") */
    if (lv.sig[r] == sig)
        return 0;
    lv.sig[r] = sig;
    return 1;
}

static void lv_draw_head(void)
{
    static const char *const PG_NAME[PG_N] = {"SOUND", "ARP", "FX"};
    uint32_t part = song.sel % LV_PARTS, bpm = lv_bpm(), split = (uint32_t)NCC_SPLIT(), ext = (uint32_t)lv_ext();
    char s[12];
    if (!lv_changed(RG_HEAD, lv_hash(lv_hash(lv_hash(lv.page * 7u + lv.sub * 3u + lv.fx, part), bpm), split * 2u + ext)))
        return;
    cv_begin(240, 26, LC_BG);
    cv_rrect(2, 2, 82, 22, 4, LC_PART[part], LC_BG);
    str_cpy(s, lv.fx ? LV_FX_PAGE[lv.sub % 6u] : PG_NAME[lv.page % PG_N], sizeof s);
    if (!lv.fx && lv.sub)
        str_cpy(s + str_len(s), " 2", 3);
    cv_text_in(2, 4, 82, &AF_M, s, LC_TEXT, LC_PART[part]);
    cv_text_on(92, 7, &AF_S, "NCC", LC_MID, LC_BG);
    cv_text_on(124, 7, &AF_S, split ? "SPLIT" : "ST", split ? LC_AMBER : LC_DIM, LC_BG);
    lv_num(s, bpm);
    cv_text_r(236, 4, &AF_M, s, ext ? LC_AMBER : LC_TEXT, LC_BG);
    cv_blit(0, 0);
}

/* a badge (a small pill), from x: its right edge + a gap */
static int32_t lv_badge(int32_t x, int32_t y, const char *s, uint16_t fill, uint16_t ink, uint16_t under)
{
    int32_t w = text_w(&AF_S, s) + 10;
    cv_rrect(x, y, w, 15, 4, fill, under);
    cv_text_on(x + 5, y, &AF_S, s, ink, fill);
    return x + w + 4;
}

static void lv_draw_pad(uint32_t part, int32_t x)
{
    const track_t *t = &trk[part];
    uint32_t on = song.sel == part, ch = mch_get(part), jx = (uint32_t)clamp(t->p[P_ED_FX], 0, 63), jn = lv_juno(part), c, ep;
    int32_t bank = t->eng_req == ENGI_FM6 ? (int32_t)t->p[P_E7] - (int32_t)FM6_OWN - 1 : -1;
    uint16_t fill = on ? LC_PDIM[part] : LC_PAD, hi = on ? LC_PART[part] : LC_LINE;
    int32_t bx;
    char s[20];
    cv_rrect(x, 0, 114, (int32_t)LH_PARTS, 7, hi, LC_BG);           /* the rim, then the pad inside it */
    cv_rrect(x + 2, 2, 110, (int32_t)LH_PARTS - 4, 6, fill, hi);
    s[0] = LV_NAME[part];
    s[1] = ' ';
    if (ch < 16u) {
        str_cpy(s + 2, "CH", 3);
        lv_num(s + 4, ch + 1u);
    } else
        str_cpy(s + 2, ch == MCH_OMNI ? "OMNI" : "OFF", 5);
    cv_text_on(x + 8, 5, &AF_S, s, on ? LC_TEXT : LC_DIM, fill);
    cv_text_r(x + 96, 5, &AF_S, lv_eng_name(t->eng_req), on ? LC_MID : LC_DIM, fill);
    cv_rect(x + 101, 9, 6, 6, midi_lit(part) ? LC_TEXT : LC_LINE);  /* MIDI IN on the part */
    if (bank >= 0 && bank < (int32_t)FM6_NBANK && (nl_bank_names(), nl.bname[bank][0]))
        str_cpy(s, nl.bname[bank], sizeof s);           /* a bank voice: its own name */
    else if (jn)
        str_cpy(s, NCC_SND[jn - 1u].name, sizeof s);
    else if (t->eng_req == ENGI_ANALOG && jx >= 4u)
        str_cpy(s, "JUNO", sizeof s);                   /* (a Juno sound after a power-on: which one is not stored) */
    else
        sound_name(t, s);
    cv_text_fit(x + 8, 22, &AF_M, s, on ? LC_TEXT : LC_MID, fill, 98);
    bx = x + 8;
    if (t->p[P_AMODE])
        bx = lv_badge(bx, 46, "ARP", on ? LC_PART[part] : LC_LINE, on ? LC_TEXT : LC_MID, fill);
    if (t->p[P_AHOLD])
        bx = lv_badge(bx, 46, "HOLD", on ? LC_TEXT : LC_LINE, on ? LC_BG : LC_MID, fill);
    if (ncd_want[part])
        (void)lv_badge(bx, 46, "DRONE", on ? LC_AMBER : LC_LINE, on ? LC_BG : LC_MID, fill);
    if (on && (lv.cpend | lv.apend)) {                  /* SELECT / ALGORITHM turning: the filters they will set */
        nl_build();
        lv_filter_peek(part, lv.cpend, lv.apend, &c, &ep);
        s[0] = '>';
        s[1] = ' ';
        lv_filter_name(s + 2, c, ep);
    } else if (on && lv.pend)
        str_cpy(s, lv.pend > 0 ? "NEXT >" : "< PREV", 7);
    else                                                /* the part's filters: category / engine */
        lv_filter_name(s, lv.cat[part], lv.ep[part]);
    cv_text_fit(x + 8, 66, &AF_S, s, on ? LC_MID : LC_DIM, fill, 98);
}

static uint32_t lv_pad_sig(uint32_t part)
{
    const track_t *t = &trk[part];
    uint32_t h = lv_hash(lv_snd(t), (uint32_t)t->p[P_E7] & 63u);
    h = lv_hash(h, (uint32_t)clamp(t->p[P_ED_FX], 0, 63) | (uint32_t)lv_juno(part) << 8);
    h = lv_hash(h, (t->p[P_AMODE] != 0) | (t->p[P_AHOLD] != 0) << 1 | ncd_want[part] << 2 | (uint32_t)midi_lit(part) << 3);
    h = lv_hash(h, song.sel == part ? (uint32_t)(uint8_t)lv.cpend | (uint32_t)(uint8_t)lv.apend << 8 : 0u);
    h = lv_hash(h, (uint32_t)lv.cat[part] | (uint32_t)lv.ep[part] << 8);
    return lv_hash(h, mch_get(part) | (song.sel == part) << 8 | (uint32_t)(song.sel == part && lv.pend) << 9);
}

static void lv_draw_parts(void)
{
    if (!lv_changed(RG_PARTS, lv_hash(lv_pad_sig(0), lv_pad_sig(1)) ^ 0x5A5Au))
        return;
    cv_begin(240, LH_PARTS, LC_BG);
    lv_draw_pad(0, 4);
    lv_draw_pad(1, 122);
    cv_blit(0, LY_PARTS);
}

/* FX held: the 16 white keys (F3 .. G5) as two rows of 8 pads, each with the effect the map gives it */
static void lv_draw_perform(void)
{
    uint32_t on = perf_held | perf_latched, p, part = song.sel % LV_PARTS, h = 0;
    for (p = 0; p < PF_KEYS; p++)
        h = lv_hash(h, perf_map[p] | ((on >> perf_map[p]) & 1u) << 8);
    if (!lv_changed(RG_PARTS, h ^ 0xF00Du) && lv.sig[RG_STEPS] == 2u)
        return;
    lv.sig[RG_STEPS] = 2u;                              /* (the steps' row is under the map: drawn again after it) */
    cv_begin(240, 102, LC_BG);
    for (p = 0; p < PF_KEYS; p++) {
        uint32_t e = perf_map[p] < PF_N ? perf_map[p] : PF_N, lit = e < PF_N && ((on >> e) & 1u);
        int32_t x = 5 + (int32_t)(p % 8u) * 29, y = p < 8u ? 52 : 4;  /* (low keys on the bottom row, as played) */
        uint16_t fill = lit ? LC_PART[part] : e < PF_N ? LC_PAD : LC_BG;
        cv_rrect(x, y, 26, 44, 4, fill, LC_BG);
        if (e < PF_N) {
            uint32_t two = LV_PF[e][1][0] != 0;
            cv_text_in(x, y + (two ? 7 : 15), 26, &AF_S, LV_PF[e][0], lit ? LC_TEXT : LC_MID, fill);
            if (two)
                cv_text_in(x, y + 23, 26, &AF_S, LV_PF[e][1], lit ? LC_TEXT : LC_MID, fill);
        }
    }
    cv_blit(0, LY_PARTS);
}

static void lv_draw_steps(void)
{
    uint32_t part = song.sel % LV_PARTS, q = div_samples(2), idx = q ? (ncc_bar / q) & 15u : 0u, i;
    uint32_t arp = trk[part].p[P_AMODE] != 0;
    if (!lv_changed(RG_STEPS, lv_hash(idx | arp << 4 | part << 5, 0xA5u) | 4u))
        return;
    cv_begin(240, 12, LC_BG);
    for (i = 0; i < 16u; i++) {
        uint16_t c = i == idx ? LC_TEXT : arp ? (i & 3u ? LC_PDIM[part] : LC_PART[part]) : (i & 3u ? LC_LINE : LC_DIM);
        cv_rrect(9 + (int32_t)i * 14, 2, 12, 8, 2, c, LC_BG);
    }
    cv_blit(0, LY_STEPS);
}

static void lv_draw_knobs(void)
{
    uint32_t part = song.sel % LV_PARTS, k, hot = lv.hot && (fm1_ms | 1u) - lv.hot_ms < LV_HOT_MS ? lv.hot : 0u;
    uint32_t h = lv_hash(lv.page * 7u + lv.sub * 3u + lv.fx, part * 16u + hot);
    int32_t v[4], lo[4], hi[4];
    for (k = 0; k < 4u; k++) {
        v[k] = lv_get(lv_knob(k), &lo[k], &hi[k]);
        h = lv_hash(h, (uint32_t)v[k]);
    }
    if (!lv_changed(RG_KNOBS, h))
        return;
    cv_begin(240, LH_KNOBS, LC_BG);
    for (k = 0; k < 4u; k++) {
        const lv_knob_t *kb = lv_knob(k);
        int32_t cx = (int32_t)k * 60, a0, a1, on = hot == k + 1u;
        uint16_t vc = on ? LC_TEXT : LC_PART[part];
        char s[16];
        if (kb->kind == LK_NONE)
            continue;
        cv_text_in(cx, 0, 60, &AF_S, lv_label(kb), on ? LC_TEXT : LC_MID, LC_BG);
        if (lo[k] < 0) {                                /* bipolar: from 12 o'clock */
            int32_t sv = v[k] * KA_END / (v[k] < 0 ? -lo[k] : (hi[k] ? hi[k] : 1));
            a0 = (sv < 0 ? sv : 0) - 8;
            a1 = (sv > 0 ? sv : 0) + 8;
        } else {
            a0 = -KA_END;
            a1 = v[k] > lo[k] && hi[k] > lo[k] ? -KA_END + (v[k] - lo[k]) * 2 * KA_END / (hi[k] - lo[k]) : -KA_END - 1;
        }
        knob_arc(cx + 30 - KNOB_BIG_R, 18, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, a0, a1, LC_LINE, vc, LC_BG);
        lv_text(kb, s);
        cv_text_in(cx, 58, 60, &AF_S, s, on ? LC_TEXT : LC_MID, LC_BG);
    }
    cv_blit(0, LY_KNOBS);
}

/* the macro popup's right side: what knob m does on the part's engine (perf_layer.c NCM_MAP) and the value its
 * first parameter has now, as the editor would show it ("CUTOFF 2.1kHz"); s: 24 bytes. -> that value (the redraw's
 * signature: it follows the knob's slide) */
static int32_t lv_macro_text(uint32_t part, uint32_t m, char *s)
{
    track_t *t = &trk[part];
    const ncm_map_t *mp = ncm_map(t);
    const param_desc_t *d;
    const char *unit;
    uint32_t id, n;
    int32_t v;
    s[0] = 0;
    if (!mp)
        return 0;
    id = mp->d[m][0].id;
    v = ncm_get(t, id);
    d = id == NCM_CEG ? 0 : track_desc(t, id);
    str_cpy(s, mp->show[m] ? mp->show[m] : d ? d->label : "", 9);
    n = str_len(s);
    s[n++] = ' ';
    if (!d) {                                           /* FM6's CEG: the offset, + slower */
        if (v > 0)
            s[n++] = '+';
        fmt_int(s + n, v);
        return v;
    }
    param_format(d, v, s + n, &unit);
    if (str_len(s) + str_len(unit) < 23u)
        str_cpy(s + str_len(s), unit, 24u - str_len(s));
    return v;
}

static void lv_draw_foot(void)
{
    static const char *const HINT[PG_N] = {"SELECT: TYPE   ALGORITHM: ENGINE", "SELECT: PAGE 2   REC: HOLD",
                                           "HOLD FX + KEY: PERFORM"};
    uint32_t seen = ncm_seen, sig, mp = (seen >> 16) & 1u, m = (seen >> 8) & 7u;
    char s[24], r[24];
    int32_t v = 0;
    if (seen != lv.seen) {
        lv.seen = seen;
        lv.pop_ms = fm1_ms | 1u;
        lv.say = 0;                                     /* (a knob after a button: the knob) */
    }
    if (lv.pop_ms && (fm1_ms | 1u) - lv.pop_ms >= LV_POP_MS)
        lv.pop_ms = 0;
    if (!lv.pop_ms)
        lv.say = 0;
    if (lv.pop_ms && !lv.say)
        v = lv_macro_text(mp, m, r);
    sig = !lv.pop_ms ? 1u << 31 | lv.page << 4 | lv.fx | lv.sub << 8 : lv.say ? 1u << 29 | lv.say_n :
          1u << 30 | (seen & 0x1FF00u) << 8 | ((uint32_t)v & 0xFFFFu);
    if (!lv_changed(RG_FOOT, sig))
        return;
    cv_begin(240, 26, LC_BG);
    cv_rect(0, 0, 240, 1, LC_LINE);
    if (!lv.pop_ms)
        cv_text_in(0, 6, 240, &AF_S, !lv.fx ? HINT[lv.page % PG_N] : lv.sub == LV_FX_PERF ?
                   "KEYS: EFFECTS   FX: CLOSE" : "FX / SELECT: PAGES   HOME: CLOSE", LC_DIM, LC_BG);
    else if (lv.say)
        cv_text_in(0, 4, 240, &AF_M, lv.say, LC_TEXT, LC_BG);
    else {                                              /* a macro: the Circuit's legend, then what it does here */
        s[0] = LV_NAME[mp];
        s[1] = ' ';
        str_cpy(s + 2, NCM_NAME[m], 20);
        cv_text_on(6, 4, &AF_M, s, LC_PART[mp], LC_BG);
        if (!r[0])                                      /* (an engine without macros: the CC as it came) */
            lv_num(r, seen & 127u);
        cv_text_r(234, 7, &AF_S, r, LC_TEXT, LC_BG);
    }
    cv_blit(0, LY_FOOT);
}

static void live_draw(void)
{
    ui.frame++;
    if (ui.uboot) {                                     /* OCT- + OCT+ held (main.c): UPDATE MODE's countdown */
        style_apply();
        draw_uboot();
        memset(lv.sig, 0, sizeof lv.sig);
        return;
    }
    if (ux.mono) {                                      /* (GREY / MONO palettes make every ramp grey: NCC has its */
        ux.mono = 0;                                    /* own colours, so its ramps are coloured; the cache anew) */
        ux.gen++;
        ui.force = 1;
    }
    if (ui.force) {                                     /* (after the countdown, an update, a palette) */
        ui.force = 0;
        lcd_fill(0, 0, 240, 240, LC_BG);
        memset(lv.sig, 0, sizeof lv.sig);
    }
    lv_draw_head();
    if (lv.fx && lv.sub == LV_FX_PERF)                  /* the FX mode's PERFORM page: the key map */
        lv_draw_perform();
    else {
        if (lv.sig[RG_STEPS] == 2u)
            lv.sig[RG_PARTS] = 0;                       /* (the map was here: the pads again) */
        lv_draw_parts();
        lv_draw_steps();
    }
    lv_draw_knobs();
    lv_draw_foot();
}
