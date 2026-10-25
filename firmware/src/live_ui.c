/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the live UI's input, milestones 1 to 3. main.c calls live_input /
 * live_leds / live_draw (live_draw.c) in place of ui_input / ui_leds / ui_draw; the rest of the UI stays built (sound
 * loads, user presets, the settings and projects, MIDI Program Change, the update countdown's screen) and is called
 * from here.
 *   two parts   L = track 1, FM6; R = track 2, ANALOG; tracks 3 and 4 muted and deaf. No sequencer: the Circuit
 *               Tracks plays them, MIDI IN TRACK, L on channel 3 and R on 4 (its MIDI 1 / MIDI 2 tracks) unless the
 *               project has TRACK already; the clock from USB (with none coming, the arp runs on BPM: seq.c)
 *   pages       SOUND (HOME), ARP, FX, SETUP (GLO), sound editing (EDIT); PLAY steps each family's pages
 *   SCL         Dual on/off on its remembered source; GLO page 2 chooses OFF/CH3/CH4
 *   EDIT        focused engine, envelope, LFO, voice/chord controls; FM6 operators (ncc_pages.c)
 *   sounds      one library of the five engines' sounds (ncc_library.c; LV_ENGS: FM6, JUNO = ANALOG, PHASE, TRIO, ACID:
 *               the ones the Circuit's knobs map onto, perf_layer.c NCM_MAP), each with a category (BASS PAD LEAD KEYS
 *               PLUCK, OTHER). On SOUND, SELECT filters the focused part's list by category, ALGORITHM (any page) by
 *               engine, each with ALL, skipping what has no sound left; once the knob has rested LV_ENG_MS the part's
 *               sound stays if it fits, else the first that does loads. PRESETS steps through what fits: a sound of
 *               another engine switches the part to it (Felucca's fade). The filters are the part's (not stored); its
 *               engine and sound are kept with the project: a project opened in NCC the first time (MIDI IN not TRACK
 *               yet) starts with L FM6, R JUNO
 *   OCT- / OCT+ the focus: L / R (its LED: OCT+ has none in PANEL_DEFAULT, so R is OCT- dark); the 27 keys play the
 *               part in focus (song.sel)
 *   ARP         the arp on for the focused part (its last mode, UP at first) and the ARP page; on the ARP page with
 *               the arp on: off. Its LED: the arp on
 *   REC         HOLD (latch) of the focused part's arp (P_AHOLD: a new chord after letting go replaces it, keys added
 *               while held add); off with no key down lets the latched chord go (ncc_seq.c). Its LED
 *   SEQ         Drone-hold of the focused part (ncc_seq.c). Its LED.   GLO + SEQ: SPLIT (the default) <-> STEREO
 *   FX          a tap opens the FX mode on its SENDS page, each tap after it the next page, the tap after the last
 *               closes it (so do HOME and ARP); PLAY steps its pages too. In it the white keys are PERFORM's effects
 *               (perform.c; seq.c kb_lock: the layer locked open), each held while its key is. Its pages: SENDS (the
 *               focused part's drive and sends), DELAY, REVERB (the bus's; KNOB 4 the part's reverb send), CHORUS (the
 *               bus's, and the part's Juno chorus and drift), INSERT (the part's: its type and A B C as the type names
 *               them), PERFORM (the key map on the screen, KNOB 1..4 its macros FILTER CRUSH THROW DEPTH, back to 0
 *               when the mode closes, but with FX LATCH)
 *   ENV / LFO   held: ECHO THROW / REV THROW (perf_layer.c ncm_throw), on every page
 *   HOME        the SOUND page
 *   PRESETS     the focused part's sounds as its filters leave them (the factory sounds, NCC's Juno sounds, FM6's voice
 *               bank, the user presets), loaded once the knob has rested LV_LOAD_MS, so turning through the list loads
 *               one sound, not each one passed. (The Juno chorus: FX > CHORUS, KNOB 3)
 *   ACID        a new ACID sound slides (GLIDE in TIME mode, eng_acid.c): LEGATO glides between overlapping notes */

#define LV_PARTS NCM_PARTS
#define LV_LOAD_MS 150u
#define LV_POP_MS 1000u
#define LV_HOT_MS 1500u                 /* a knob ring stays lit after its turn */
#define LV_ENG_MS 300u
#define LV_ACID_GLIDE 24                /* a new ACID sound's slide (GLIDE, TIME mode) */
#include "ncc_presets.c"                                /* (NCC's own sounds: ncc_snd_load) */

static const uint8_t LV_ENG[LV_PARTS] = {ENGI_FM6, ENGI_ANALOG};   /* (a part with an engine not in LV_ENGS) */
static const uint8_t LV_ENGS[] = {ENGI_FM6, ENGI_ANALOG, ENGI_PHASE, ENGI_TRIO, ENGI_ACID,
                                  ENGI_PHYS, 8, 5, 3, 7, 11, ENGI_SAMPLE,
#if NCC_LOOP
                                  ENGI_LOOP,           /* (core.h NCC_LOOP: coming soon) */
#endif
};
static const char *const LV_ENG_NAME[] = {"FM6", "JUNO", "PHASE", "TRIO", "ACID",
                                          "PHYS", "GRAIN", "VOICE", "LOFI", "WHEEL", "NOISE", "SAMPLE",
#if NCC_LOOP
                                          "LOOP",
#endif
};
static int32_t lv_eng_pos(uint32_t e)                  /* e's place in LV_ENGS, -1 none */
{
    uint32_t i;
    for (i = 0; i < NELEM(LV_ENGS); i++)
        if (LV_ENGS[i] == e)
            return (int32_t)i;
    return -1;
}
static const char *lv_eng_name(uint32_t e)
{
    int32_t i = lv_eng_pos(e);
    return i >= 0 ? LV_ENG_NAME[i] : ENGINES[eng_idx(e)]->name;
}
static const char LV_NAME[LV_PARTS] = {'L', 'R'};

enum { PG_SOUND, PG_ARP, PG_FX, PG_GLO, PG_EDIT, PG_N };
enum { LK_NONE, LK_TP, LK_G, LK_OCT, LK_AMODE, LK_ARATE, LK_ITYPE, LK_PERF, LK_INS, LK_JUNO, LK_DRIFT, LK_DRONE,
       LK_CHANNEL, LK_OUTPUT, LK_DUAL, LK_DRONESTATE, LK_FOP };
static const char *const LV_DRONE[] = {"STACK", "REPLACE"};
#define LV_NSUBMAX 6
typedef struct {
    uint8_t kind, id;                   /* LK_*, the P_* / G_* id (LK_PERF: perf_k's index) */
    const char *label;
} lv_knob_t;
/* KNOB 1..4 per page and its pages (PLAY) */
#define LK_EMPTY {LK_NONE, 0, ""}
static const lv_knob_t LV_KNOBS[PG_N][LV_NSUBMAX][4] = {
    [PG_SOUND] = {{{LK_TP, P_LEVEL, "LEVEL"}, {LK_OCT, 0, "OCTAVE"}, {LK_TP, P_VOICE, "VOICE"}, {LK_TP, P_GLIDE, "GLIDE"}}},
    [PG_ARP] = {{{LK_AMODE, P_AMODE, "MODE"}, {LK_ARATE, P_ARATE, "RATE"}, {LK_TP, P_AOCT, "OCTAVES"},
                 {LK_TP, P_AGATE, "GATE"}},
                {{LK_TP, P_ASWING, "SWING"}, {LK_TP, P_APROB, "PROB"}, {LK_TP, P_AORDER, "ORDER"}, {LK_DRONE, 0, "DRONE"}}},
    [PG_FX] = {{{LK_TP, P_DIST, "DRIVE"}, {LK_TP, P_CHOR, "CHORUS"}, {LK_TP, P_DLY, "DELAY"}, {LK_TP, P_REV, "REVERB"}},
               {{LK_G, G_DTIME, "TIME"}, {LK_G, G_DFDBK, "FEEDBK"}, {LK_G, G_DCOLOR, "COLOR"}, {LK_G, G_DMIX, "MIX"}},
               {{LK_G, G_RTYPE, "TYPE"}, {LK_G, G_RSIZE, "SIZE"}, {LK_G, G_RDAMP, "DAMP"}, {LK_TP, P_REV, "SEND"}},
               {{LK_G, G_CRATE, "RATE"}, {LK_G, G_CDEPTH, "DEPTH"}, {LK_JUNO, 0, "JUNO"}, {LK_DRIFT, 0, "DRIFT"}},
               {{LK_ITYPE, P_ITYPE, "TYPE"}, {LK_INS, P_IA, ""}, {LK_INS, P_IB, ""}, {LK_INS, P_IC, ""}},
               {{LK_PERF, 0, "FILTER"}, {LK_PERF, 1, "CRUSH"}, {LK_PERF, 2, "THROW"}, {LK_PERF, 3, "DEPTH"}}},
};
static const uint8_t LV_NSUB[PG_N] = {1, 2, 6};        /* subpages per family (GLO/EDIT: lv_nsub) */
static const char *const LV_FX_PAGE[6] = {"SENDS", "DELAY", "REVERB", "CHORUS", "INSERT", "PERFORM"};
#define LV_FX_PERF 5u                   /* the FX mode's PERFORM page (the key map on the screen), its last */
static const uint8_t LV_RATES[] = {0, 1, 4, 2, 5, 3};   /* RATE on KNOB 2: 1/4 1/8 8T 1/16 16T 1/32 (N_DIV) */

static struct {
    uint8_t ok;                         /* lv_setup done */
    uint8_t page, sub;                  /* PG_*, its subpage (PLAY) */
    uint8_t page_down, page_family, page_sub, page_part; /* pending short/long PLAY gesture */
    uint32_t page_ms;
    uint8_t prev;                       /* the page the FX mode was opened from */
    uint8_t fx;                         /* the FX mode (its layer: the keys PERFORM's) */
    uint8_t hot;                        /* the knob turned last + 1 (0 none) */
    uint32_t hot_ms;                    /* .. when, fm1_ms | 1 */
    uint8_t amode[LV_PARTS];            /* the arp's mode while off (ARP on brings it back) */
    int16_t pend;                       /* PRESETS detents not loaded yet */
    int8_t cpend, apend;                /* SELECT detents on SOUND / ALGORITHM detents: the filter not applied yet */
    uint32_t fpend_ms;                  /* the last of them, fm1_ms | 1 */
    uint8_t cat[LV_PARTS], ep[LV_PARTS];   /* the part's filters: NC_* category, its engine in LV_ENGS + 1 (0: ALL) */
    uint32_t snd[LV_PARTS];             /* the part's sound as last seen (a new ACID sound: its slide) */
    uint32_t pend_ms;                   /* the last of them, fm1_ms | 1 */
    uint32_t seen;                      /* the ncm_seen the foot shows */
    uint32_t pop_ms;                    /* the foot's popup since (fm1_ms | 1), 0 none */
    uint32_t sig[8];                    /* what is on the screen, per region (live_draw.c; 0: redraw) */
    const char *say;                    /* the foot's popup is this (a button's), not the knob's */
    char say_b[24];
    uint8_t say_n;                      /* + 1 each time: the foot redraws */
    uint8_t juno[LV_PARTS];             /* the Juno sound k + 1 the part has (0: not one, or not known) */
    uint32_t jsig[LV_PARTS];            /* the part's engine / preset / user when it was loaded */
} lv;

static uint32_t lv_snd(const track_t *t) { return t->eng_req | (uint32_t)t->preset << 8 | (uint32_t)t->user << 16; }
static void lv_say(const char *a, const char *b)       /* the foot's popup: a + b */
{
    str_cpy(lv.say_b, a, sizeof lv.say_b);
    str_cpy(lv.say_b + str_len(lv.say_b), b, sizeof lv.say_b - str_len(lv.say_b));
    lv.say = lv.say_b;
    lv.say_n++;
    lv.pop_ms = fm1_ms | 1u;
}
static void lv_say_part(const char *what, int on)     /* "L HOLD ON" */
{
    char b[16];
    b[0] = LV_NAME[song.sel % LV_PARTS];
    b[1] = ' ';
    str_cpy(b + 2, what, sizeof b - 2u);
    lv_say(b, on ? " ON" : " OFF");
}
/* the Juno sound part has now: k + 1, 0 none (anything else loaded since: forgotten) */
static uint32_t lv_juno(uint32_t part)
{
    if (lv.juno[part] && lv_snd(&trk[part]) != lv.jsig[part])
        lv.juno[part] = 0;
    return lv.juno[part];
}

/* the first pass (after the power-on restore): two parts, their engines, the channels, nothing for the sequencer */
static void lv_setup(void)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (i < LV_PARTS && lv_eng_pos(t->eng_req) < 0)
            set_engine_of(t, LV_ENG[i]);               /* (an engine NCC does not offer: the part's default) */
        track_defaults_steps(t);                        /* the Circuit plays: no pattern of ours starts with its clock */
        if (i >= LV_PARTS)
            t->p[P_MUTE] = 1;
        if (i < LV_PARTS)
            lv.amode[i] = (uint8_t)(t->p[P_AMODE] ? t->p[P_AMODE] : 1);   /* (UP) */
    }
    if (song.g[G_ROUTE] != ROUTE_TRACK) {               /* a project NCC has not had yet: its parts, channels */
        set_engine_of(&trk[0], LV_ENG[0]);
        set_engine_of(&trk[1], LV_ENG[1]);
        song.g[G_ROUTE] = ROUTE_TRACK;
        mch_set(0, 2u);                                 /* channel 3: the Circuit's MIDI 1 */
        mch_set(1, 3u);                                 /* channel 4: its MIDI 2 */
        mch_set(2, MCH_OFF);
        mch_set(3, MCH_OFF);
    }
    if (!song.g[G_CLOCK])
        song.g[G_CLOCK] = 1;                            /* USB: the Circuit's clock (none coming: BPM, seq.c) */
    if (song.sel >= LV_PARTS)
        track_select(0);
    lv.ok = 1;
}

#include "ncc_library.c"                               /* (the sounds by category: nl_*) */

/* the pos-th sound (from 0) that fits the filters, on the focused part */
static void lv_load_nth(uint32_t part, uint32_t c, uint32_t ep, uint32_t pos)
{
    uint32_t i;
    for (i = 0; i < nl.n; i++)
        if (nl_fits(i, c, ep) && !pos--)
            break;
    if (i == nl.n)
        return;
    nl_load(i);
    lv.juno[part] = (uint8_t)(nl.s[i].kind == NLK_NCC ? nl.s[i].k + 1u : 0u);
    lv.jsig[part] = lv_snd(TSEL);
}

/* PRESETS d detents on: where it lands among the *n sounds the part's filters leave (nl_build first); *cur its sound's
 * place there (*n: not among them). Not round: it stops at the first and the last (not among them: from the first /
 * the last) */
static uint32_t lv_list_peek(uint32_t part, int32_t d, uint32_t *n, uint32_t *cur)
{
    uint32_t c = lv.cat[part % LV_PARTS], ep = lv.ep[part % LV_PARTS], i, pos = 0;
    int32_t f = nl_find(part, lv_juno(part)), to;
    *n = nl_count(c, ep);
    *cur = *n;
    for (i = 0; i < nl.n; i++)
        if (nl_fits(i, c, ep)) {
            if ((int32_t)i == f)
                *cur = pos;
            pos++;
        }
    if (!*n)
        return 0;
    to = *cur < *n ? (int32_t)*cur + d : d > 0 ? d - 1 : (int32_t)*n + d;
    return (uint32_t)clamp(to, 0, (int32_t)*n - 1);
}

/* PRESETS: d sounds on from the focused part's, among those its filters leave */
static void lv_list_go(int32_t d)
{
    uint32_t part = song.sel, n, cur, pos;
    if (part >= LV_PARTS || !d)
        return;
    nl_build();
    pos = lv_list_peek(part, d, &n, &cur);
    if (n && pos != cur)
        lv_load_nth(part, lv.cat[part], lv.ep[part], pos);
}

/* the filters dc categories and de engines on from the part's, past those with no sound left (nl_build first). Not
 * round: they stop at ALL and at the last */
static void lv_filter_peek(uint32_t part, int32_t dc, int32_t de, uint32_t *c, uint32_t *ep)
{
    uint32_t t, top = NELEM(LV_ENGS) + 1u;
    *c = lv.cat[part] % NC_N;
    *ep = lv.ep[part] % top;
    for (; dc; dc += dc > 0 ? -1 : 1) {
        for (t = *c; dc > 0 ? t + 1u < NC_N : t > 0u;)
            if (nl_count(t += dc > 0 ? 1u : (uint32_t)-1, *ep))
                break;
        if (nl_count(t, *ep))
            *c = t;
    }
    for (; de; de += de > 0 ? -1 : 1) {
        for (t = *ep; de > 0 ? t + 1u < top : t > 0u;)
            if (nl_count(*c, t += de > 0 ? 1u : (uint32_t)-1))
                break;
        if (nl_count(*c, t))
            *ep = t;
    }
}
static void lv_num(char *b, uint32_t v);                /* (live_draw.c) */
/* the entry the pos-th sound (from 0) that fits the filters is, nl.n none (nl_build first) */
static uint32_t lv_nth(uint32_t c, uint32_t ep, uint32_t pos)
{
    uint32_t i;
    for (i = 0; i < nl.n; i++)
        if (nl_fits(i, c, ep) && !pos--)
            return i;
    return nl.n;
}
/* the SOUND page's line for the focused part (s holds 40; nl_build first): what SELECT / ALGORITHM turn to and whether
 * they are at an end, where PRESETS lands (and at an end), else "TYPE PAD  ENGINE ALL  3/10" */
static void lv_sound_line(char *s)
{
    uint32_t part = song.sel % LV_PARTS, c, ep, c2, ep2, n, cur, pos;
    char b[12];
    s[0] = 0;
    if (lv.cpend | lv.apend) {
        lv_filter_peek(part, lv.cpend, lv.apend, &c, &ep);
        lv_filter_peek(part, lv.cpend + (lv.cpend > 0) - (lv.cpend < 0), lv.apend + (lv.apend > 0) - (lv.apend < 0), &c2, &ep2);
        str_cpy(s, lv.cpend ? "TYPE: " : "ENGINE: ", 9);
        str_cpy(s + str_len(s), lv.cpend ? NC_NAME[c % NC_N] : ep ? LV_ENG_NAME[(ep - 1u) % NELEM(LV_ENGS)] : "ALL", 8);
        str_cpy(s + str_len(s), "  ", 3);
        lv_num(b, nl_count(c, ep));
        str_cpy(s + str_len(s), b, 5);
        str_cpy(s + str_len(s), (lv.cpend ? c2 == c : ep2 == ep) ? " SOUNDS  (END)" : " SOUNDS", 15);
        return;
    }
    pos = lv_list_peek(part, lv.pend, &n, &cur);
    if (lv.pend) {
        str_cpy(s, "SOUND ", 7);
        lv_num(b, n ? pos + 1u : 0u);
        str_cpy(s + str_len(s), b, 5);
        str_cpy(s + str_len(s), " / ", 4);
        lv_num(b, n);
        str_cpy(s + str_len(s), b, 5);
        if (n && (pos == 0u || pos == n - 1u) && pos == lv_list_peek(part, lv.pend + (lv.pend > 0 ? 1 : -1), &n, &cur))
            str_cpy(s + str_len(s), pos ? "  (LAST)" : "  (FIRST)", 10);
        return;
    }
    str_cpy(s, "TYPE ", 6);
    str_cpy(s + 5, NC_NAME[lv.cat[part] % NC_N], 8);
    str_cpy(s + str_len(s), "   ENGINE ", 11);
    str_cpy(s + str_len(s), lv.ep[part] ? LV_ENG_NAME[(lv.ep[part] - 1u) % NELEM(LV_ENGS)] : "ALL", 8);
    if (cur < n) {
        str_cpy(s + str_len(s), "   ", 4);
        lv_num(b, cur + 1u);
        str_cpy(s + str_len(s), b, 5);
        str_cpy(s + str_len(s), "/", 2);
        lv_num(b, n);
        str_cpy(s + str_len(s), b, 5);
    }
}
/* "PAD / ALL" (b holds 16) */
static void lv_filter_name(char *b, uint32_t c, uint32_t ep)
{
    str_cpy(b, NC_NAME[c % NC_N], 6);
    str_cpy(b + str_len(b), " / ", 4);
    str_cpy(b + str_len(b), ep ? LV_ENG_NAME[(ep - 1u) % NELEM(LV_ENGS)] : "ALL", 6);
}

/* SELECT on SOUND, ALGORITHM: the focused part's filters on; its sound kept if it fits, else the first that does */
static void lv_filter_go(int32_t dc, int32_t de)
{
    uint32_t part = song.sel, c, ep;
    int32_t f;
    char b[24];
    if (part >= LV_PARTS)
        return;
    nl_build();
    lv_filter_peek(part, dc, de, &c, &ep);
    lv.cat[part] = (uint8_t)c;
    lv.ep[part] = (uint8_t)ep;
    f = nl_find(part, lv_juno(part));
    if (f < 0 || !nl_fits((uint32_t)f, c, ep))
        lv_load_nth(part, c, ep, 0);
    b[0] = LV_NAME[part];
    b[1] = ' ';
    lv_filter_name(b + 2, c, ep);
    lv_say(b, "");
}

static void lv_focus(uint32_t part)
{
    if (lv.pend) {                                      /* a sound picked on the other part: loaded there first */
        lv_list_go(lv.pend);
        lv.pend = 0;
    }
    if (lv.cpend | lv.apend) {                          /* .. a filter too */
        lv_filter_go(lv.cpend, lv.apend);
        lv.cpend = lv.apend = 0;
    }
    track_select(part);
}

#include "ncc_pages.c"                                 /* settings and sound editing through NCC's own adapters */

/* ---------------------------------------------------------- the knobs --- */
static const lv_knob_t *lv_knob(uint32_t k)
{
    if (lv.page == PG_GLO)
        return &LV_SETTINGS[lv.sub % NELEM(LV_SETTINGS)][k & 3u];
    if (lv.page == PG_EDIT)
        return &LV_EDIT[lv_edit_index()][k & 3u];
    return &LV_KNOBS[lv.page % PG_N][lv.sub % LV_NSUBMAX][k & 3u];
}
static const param_desc_t *lv_desc(const lv_knob_t *kb)
{
    if (kb->kind == LK_INS)                             /* A B C as the INSERT's type names them */
        return ins_desc(TSEL->p[P_ITYPE], kb->id - P_IA);
    return kb->kind == LK_G ? &GP[kb->id] : track_desc(TSEL, kb->id);
}
static const char *lv_label(const lv_knob_t *kb)
{
    const char *l = kb->kind == LK_INS || (lv.page == PG_EDIT && kb->kind == LK_TP) ? lv_desc(kb)->label : kb->label;
    if (kb->kind == LK_FOP && kb->id == FO_CRS && fop_bytes(song.sel)[FP_MODE])
        l = "FIXED";
    return l ? l : "";                                  /* (a page's empty places) */
}
/* the knob's value and its range */
static int32_t lv_get(const lv_knob_t *kb, int32_t *lo, int32_t *hi)
{
    const param_desc_t *d;
    uint32_t i;
    switch (kb->kind) {
    case LK_OCT:
        *lo = -3;
        *hi = 3;
        return song.octave;
    case LK_PERF:
        *lo = kb->id ? 0 : -100;
        *hi = 100;
        return perf_k[kb->id & 3u];
    case LK_AMODE:
        *lo = 1;
        *hi = (int32_t)NELEM(N_AMODE) - 1;
        return TSEL->p[P_AMODE] ? TSEL->p[P_AMODE] : lv.amode[song.sel % LV_PARTS];
    case LK_ARATE:
        *lo = 0;
        *hi = (int32_t)NELEM(LV_RATES) - 1;
        for (i = 0; i < NELEM(LV_RATES) && LV_RATES[i] != TSEL->p[P_ARATE]; i++)
            ;
        return i < NELEM(LV_RATES) ? (int32_t)i : 3;    /* (a RATE outside the six: shown at 1/16) */
    case LK_JUNO:
        *lo = 0;
        *hi = 3;
        return clamp(TSEL->p[P_ED_FX], 0, 63) & 3;
    case LK_DRIFT:
        *lo = 0;
        *hi = 15;
        return (clamp(TSEL->p[P_ED_FX], 0, 63) >> 2) & 15;
    case LK_DRONE:
        *lo = 0;
        *hi = 1;
        return (int32_t)ncd_mode(song.sel % LV_PARTS);
    case LK_CHANNEL:
        *lo = 0; *hi = 17;
        i = mch_get(kb->id);
        return (int32_t)(i < 16u ? i : i == MCH_OMNI ? 16u : 17u);
    case LK_OUTPUT:
        *lo = 0; *hi = 1;
        return NCC_SPLIT() ? 0 : 1;
    case LK_DUAL:
        *lo = 0; *hi = 2;
        return (int32_t)ncc_dual_mode();
    case LK_DRONESTATE:
        *lo = 0; *hi = 2;
        return ncd_want[kb->id] ? 1 + (int32_t)ncd_mode(kb->id) : 0;
    case LK_FOP:
        *lo = 0;
        *hi = kb->id == FO_OP ? 5 : kb->id == FO_STG ? 3 : (int32_t)fm6_max(fop_byte(kb->id));
        return kb->id == FO_OP ? fop_op : kb->id == FO_STG ? fop_stg : fm6_patch[song.sel][fop_byte(kb->id)];
    case LK_NONE:
        *lo = *hi = 0;
        return 0;
    default:
        d = lv_desc(kb);
        *lo = d->min;
        *hi = d->max;
        return kb->kind == LK_G ? song.g[kb->id] : TSEL->p[kb->id];
    }
}
/* the knob's value as text (b holds 16) */
static void lv_text(const lv_knob_t *kb, char *b)
{
    int32_t lo, hi, v = lv_get(kb, &lo, &hi);
    const char *unit = 0;
    b[0] = 0;
    switch (kb->kind) {
    case LK_NONE:
        return;
    case LK_OCT:
    case LK_PERF:
        if (v > 0)
            *b++ = '+';
        fmt_int(b, v);
        return;
    case LK_AMODE:
        str_cpy(b, N_AMODE[clamp(v, 0, (int32_t)NELEM(N_AMODE) - 1)], 8);
        return;
    case LK_ARATE:
        str_cpy(b, N_DIV[LV_RATES[v]], 8);
        return;
    case LK_JUNO:
        str_cpy(b, JC_NAME[v & 3], 8);
        return;
    case LK_DRIFT:
        fmt_int(b, v);
        return;
    case LK_DRONE:
        str_cpy(b, LV_DRONE[v], 16);
        return;
    case LK_CHANNEL:
        if (v >= 16)
            str_cpy(b, v == 16 ? "OMNI" : "OFF", 16);
        else
            fmt_int(b, v + 1);
        return;
    case LK_OUTPUT:
        str_cpy(b, v ? "STEREO" : "SPLIT", 16);
        return;
    case LK_DUAL:
        str_cpy(b, v == 1 ? "CH3" : v == 2 ? "CH4" : "OFF", 16);
        return;
    case LK_DRONESTATE:
        str_cpy(b, v ? LV_DRONE[v - 1] : "OFF", 16);
        return;
    case LK_FOP:
        lv_fop_text(kb->id, v, b);
        return;
    default:
        param_format(lv_desc(kb), v, b, &unit);
        if (unit && str_len(b) + str_len(unit) < 15u)
            str_cpy(b + str_len(b), unit, 16u - str_len(b));
    }
}
/* knob k turned s detents */
static void lv_turn(uint32_t k, int32_t s)
{
    const lv_knob_t *kb = lv_knob(k);
    track_t *t = TSEL;
    uint32_t part = song.sel % LV_PARTS;
    int32_t lo, hi, from = lv_get(kb, &lo, &hi), v;
    if (kb->kind == LK_NONE)
        return;
    lv.hot = (uint8_t)(k + 1u);
    lv.hot_ms = fm1_ms | 1u;
    if (hi - lo > 60 && !(kb->kind == LK_INS && lv_desc(kb)->names))
        s *= 2;                                         /* (0..127 in two turns or so) */
    v = clamp(from + s, lo, hi);
    if (v == from)
        return;
    if (lv.page == PG_GLO || lv.page == PG_EDIT)
        lv.pop_ms = 0, lv.say = 0;                     /* a new value must not leave an old button message below it */
    if (lv.page == PG_EDIT && kb->kind == LK_TP)
        edit_undo_take(t, kb->id, UNDO_SOUND);
    switch (kb->kind) {
    case LK_OCT:
        song.octave = (int8_t)v;
        break;
    case LK_PERF:
        perf_k[kb->id & 3u] = (int8_t)v;
        break;
    case LK_AMODE:
        lv.amode[part] = (uint8_t)v;
        if (t->p[P_AMODE])
            t->p[P_AMODE] = (int16_t)v;
        break;
    case LK_ARATE:
        t->p[P_ARATE] = LV_RATES[v];
        break;
    case LK_DRONE:
        ncd_set_mode(part, (uint32_t)v);
        break;
    case LK_CHANNEL:
        mch_set(kb->id, v < 16 ? (uint32_t)v : v == 16 ? MCH_OMNI : MCH_OFF);
        break;
    case LK_OUTPUT:
        song.g[G_ENGGO] = (int16_t)(v ? NCC_SPLIT_DEFAULT : !NCC_SPLIT_DEFAULT);
        break;
    case LK_DUAL:
        ncc_set_dual((uint32_t)v);
        break;
    case LK_DRONESTATE:
        if (v)
            ncd_set_mode(kb->id, (uint32_t)(v - 1));
        ncd_want[kb->id] = (uint8_t)(v != 0);
        break;
    case LK_FOP:
        if (kb->id == FO_OP)
            fop_op = (uint8_t)v;
        else if (kb->id == FO_STG)
            fop_stg = (uint8_t)v;
        else
            fop_write(fop_byte(kb->id), (uint32_t)v);
        break;
    case LK_ITYPE:
        t->p[P_ITYPE] = (int16_t)v;
        if (v && !t->p[P_IMIX])
            t->p[P_IMIX] = 100;                         /* (a type with MIX 0 is not heard) */
        break;
    case LK_JUNO:
    case LK_DRIFT: {
        int32_t x = clamp(t->p[P_ED_FX], 0, 63);
        t->p[P_ED_FX] = (int16_t)(kb->kind == LK_JUNO ? (x & ~3) | v : (x & 3) | v << 2);
        break;
    }
    case LK_G: {
        const param_desc_t *d = lv_desc(kb);
        song.g[kb->id] = (int16_t)(d->names ? enum_step(d, from, v) : v);
        break;
    }
    default: {
        const param_desc_t *d = lv_desc(kb);
        t->p[kb->id] = (int16_t)(d->names ? enum_step(d, from, v) : v);
        if (lv.page == PG_EDIT && t->eng_req == ENGI_FM6 && kb->id == P_E7 && t->p[P_E7] != from)
            ncc_sends_zero(t);                         /* choosing another FM6 slot is a sound load */
    }
    }
    if (lv.page == PG_EDIT && kb->kind == LK_TP)
        motion_undo_done(t);
}

/* ---------------------------------------------------------- the panel --- */
static void lv_arp_button(void)                        /* ARP: on (and its page); on its page: off */
{
    track_t *t = TSEL;
    uint32_t part = song.sel % LV_PARTS;
    if (!t->p[P_AMODE]) {
        t->p[P_AMODE] = lv.amode[part] ? lv.amode[part] : 1;
        lv.page = PG_ARP;
        lv.sub = 0;
        lv_say_part("ARP", 1);
    } else if (lv.page == PG_ARP && !lv.fx) {
        lv.amode[part] = (uint8_t)t->p[P_AMODE];
        t->p[P_AMODE] = 0;
        lv_say_part("ARP", 0);
    } else {
        lv.page = PG_ARP;
        lv.sub = 0;
    }
}

static void live_input(void)
{
    uint32_t pressed = fm1_input_edges(0), k, btn = fm1_in.buttons, fxb = 1u << panel.btn[B_FX];
    int32_t s;
    (void)fm1_input_note_edges();                       /* (the keys play in the audio ISR: seq.c) */
    midi_take();                                        /* what ui_input keeps up besides the panel */
    midi_pc_poll();
    fm6_poll();
    loop_user_poll();                                   /* (LOOP: a user loop picked, decoded a piece a pass) */
#if !FELUCCA_FM4
    for (k = 0; k < NTRK; k++)
        if (trk[k].eng_req == ENGI_DIGITAL)
            fm4_track(&trk[k]);
#endif
    perf_latch_on = fx_latch & 1u;
    fx_map_sync();
    fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;
    scope_mix = (ui_prefs2 & (PREF_SCOPE_MIX >> 16)) != 0u;
    rp_apply();
    usb_serial_apply();
    settings_later_poll();
    if (!lv.ok)
        lv_setup();
    kb_mask = perf_mask = fxb;                          /* (FX held works too, as in Felucca) */
    if ((pressed >> panel.btn[B_FX]) & 1u) {            /* FX: the FX mode opens on SENDS, a tap the next page, */
        if (lv.page == PG_FX && lv.sub + 1u >= LV_NSUB[PG_FX])   /* after the last (PERFORM) it closes */
            lv.page = lv.prev, lv.sub = 0;
        else if (lv.page == PG_FX)
            lv.sub++;
        else
            lv.prev = lv.page, lv.page = PG_FX, lv.sub = 0;
    }
    if ((pressed >> panel.btn[B_GLO]) & 1u)
        lv_open(PG_GLO);
    if ((pressed >> panel.btn[B_EDIT]) & 1u)
        lv_open(PG_EDIT);
    if ((pressed >> panel.btn[B_HOME]) & 1u)
        lv.page = PG_SOUND, lv.sub = 0;
    if ((pressed >> panel.btn[B_SCL]) & 1u) {
        ncc_set_dual(ncc_dual_mode() ? 0u : ncc_dual_source());
        lv_say(ncc_dual_mode() == 1u ? "DUAL: CH3 TO L+R" : ncc_dual_mode() == 2u ? "DUAL: CH4 TO L+R" : "DUAL OFF", "");
    }
    if ((pressed >> panel.btn[B_ARP]) & 1u)
        lv_arp_button();
    if (lv.fx && lv.page != PG_FX && !perf_latch_on)
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;   /* (the macros with the mode: off with it) */
    lv.fx = lv.page == PG_FX;
    kb_lock = lv.fx ? 2u : 0u;                          /* the FX mode: the keys PERFORM's, no button held (seq.c) */
    (void)btn;
    ncm_throw = (uint8_t)(((btn >> panel.btn[B_ENV]) & 1u) | ((btn >> panel.btn[B_LFO]) & 1u) << 1);
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        lv_focus(0);
    if ((pressed >> panel.btn[B_OCTUP]) & 1u)
        lv_focus(1);
    if ((pressed >> panel.btn[B_REC]) & 1u && song.sel < LV_PARTS) {
        TSEL->p[P_AHOLD] = (int16_t)!TSEL->p[P_AHOLD];
        lv_say_part("HOLD", TSEL->p[P_AHOLD]);
    }
    if ((pressed >> panel.btn[B_SEQ]) & 1u) {
        if ((btn >> panel.btn[B_GLO]) & 1u) {           /* GLO + SEQ: ST <-> SPLIT */
            song.g[G_ENGGO] = (int16_t)(NCC_SPLIT() == NCC_SPLIT_DEFAULT);   /* (core.h) */
            lv_say(NCC_SPLIT() ? "SPLIT: L LEFT, R RIGHT" : "STEREO", "");
        } else if (song.sel < LV_PARTS) {               /* Drone-hold */
            ncd_want[song.sel] = (uint8_t)!ncd_want[song.sel];
            lv_say_part("DRONE", ncd_want[song.sel]);
        }
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {
        lv.pend = (int16_t)clamp(lv.pend + s, -999, 999);
        lv.pend_ms = fm1_ms | 1u;
    }
    if (lv.pend && (fm1_ms | 1u) - lv.pend_ms >= LV_LOAD_MS) {
        lv_list_go(lv.pend);
        lv.pend = 0;
    }
    if ((s = panel_enc(EN_ALGO)) != 0 && song.sel < LV_PARTS) {   /* the engine filter (after a rest) */
        lv.apend = (int8_t)clamp(lv.apend + s, -12, 12);
        lv.fpend_ms = fm1_ms | 1u;
    }
    lv_page_button(pressed, btn);                      /* PLAY: short next / long previous; never transport */
    if ((s = panel_enc(EN_SELECT)) != 0 && lv.page == PG_SOUND && song.sel < LV_PARTS) {
        lv.cpend = (int8_t)clamp(lv.cpend + s, -12, 12);
        lv.fpend_ms = fm1_ms | 1u;
    }
    if ((lv.cpend | lv.apend) && (fm1_ms | 1u) - lv.fpend_ms >= LV_ENG_MS) {
        lv_filter_go(lv.cpend, lv.apend);
        lv.cpend = lv.apend = 0;
    }
    for (k = 0; k < LV_PARTS; k++) {                    /* a new ACID sound slides */
        track_t *t = &trk[k];
        uint32_t snd = lv_snd(t);
        if (snd != lv.snd[k] && t->eng_req == ENGI_ACID) {
            t->p[P_GLMODE] = 1;
            if (!t->p[P_GLIDE])
                t->p[P_GLIDE] = LV_ACID_GLIDE;
        }
        lv.snd[k] = snd;
    }
    if (lv.sub >= lv_nsub(lv.page))
        lv.sub = (uint8_t)(lv_nsub(lv.page) - 1u);       /* a sound/focus change may hide engine-specific edit pages */
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0)
            lv_turn(k, s);
}

static void live_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint32_t c, part = song.sel % LV_PARTS;
    const track_t *t = &trk[part];
    static uint8_t ready;
    if (fm1_led_anim_on())                              /* (the power-on sweep has them) */
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[song.sel ? B_OCTUP : B_OCTDN], 1);   /* (PANEL_DEFAULT knows no LED of OCT+: R shows as OCT- dark) */
    led_put(nl, panel.btn[B_ARP], t->p[P_AMODE] != 0);
    led_put(nl, panel.btn[B_REC], t->p[P_AHOLD] != 0);
    led_put(nl, panel.btn[B_SEQ], ncd_want[part] != 0);
    led_put(nl, panel.btn[B_FX], lv.fx || perf_act != 0u);
    led_put(nl, panel.btn[B_SCL], ncc_dual_mode() != 0u);
    led_put(nl, panel.btn[B_GLO], lv.page == PG_GLO);
    led_put(nl, panel.btn[B_EDIT], lv.page == PG_EDIT);
    led_put(nb, panel.btn[B_ENV], (ncm_throw & 1u) != 0);
    led_put(nb, panel.btn[B_LFO], (ncm_throw & 2u) != 0);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = fm1_led_mid[c] = 0;
        fm1_led_breath[c] = nb[c];
        fm1_led[c] = nl[c] | nb[c];
    }
}

#include "live_draw.c"                                  /* the screen */
