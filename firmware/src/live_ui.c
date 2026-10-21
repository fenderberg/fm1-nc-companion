/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the live UI's input, milestones 1 to 3. main.c calls live_input /
 * live_leds / live_draw (live_draw.c) in place of ui_input / ui_leds / ui_draw; the rest of the UI stays built (sound
 * loads, user presets, the settings and projects, MIDI Program Change, the update countdown's screen) and is called
 * from here.
 *   two parts   L = track 1, FM6; R = track 2, ANALOG; tracks 3 and 4 muted and deaf. No sequencer: the Circuit
 *               Tracks plays them, MIDI IN TRACK, L on channel 3 and R on 4 (its MIDI 1 / MIDI 2 tracks) unless the
 *               project has TRACK already; the clock from USB (with none coming, the arp runs on BPM: seq.c)
 *   pages       SOUND (HOME), ARP, FX: KNOB 1..4 set the focused part's (LV_KNOBS); SELECT turns to page 2 of ARP / FX
 *   OCT- / OCT+ the focus: L / R (its LED: OCT+ has none in PANEL_DEFAULT, so R is OCT- dark); the 27 keys play the
 *               part in focus (song.sel)
 *   ARP         the arp on for the focused part (its last mode, UP at first) and the ARP page; on the ARP page with
 *               the arp on: off. Its LED: the arp on
 *   REC         HOLD (latch) of the focused part's arp (P_AHOLD: a new chord after letting go replaces it, keys added
 *               while held add); off with no key down lets the latched chord go (ncc_seq.c). Its LED
 *   SEQ         Drone-hold of the focused part (ncc_seq.c). Its LED.   GLO + SEQ: ST <-> SPLIT (juno_chorus.c)
 *   FX          the FX page; held: the white keys are PERFORM's effects (perform.c, the key map on the screen) and
 *               KNOB 1..4 its macros FILTER CRUSH THROW DEPTH (back to 0 when FX is let go, but with FX LATCH)
 *   ENV / LFO   held: ECHO THROW / REV THROW (perf_layer.c ncm_throw), on every page
 *   HOME        the SOUND page
 *   PRESETS     the focused part's sounds (on ANALOG first NCC's Juno sounds, ncc_presets.c; then the engine's presets,
 *               then its user presets), loaded once the knob has rested LV_LOAD_MS, so turning through the list loads
 *               one sound, not each one passed
 *   ALGORITHM   FM6: SLOT (F1..F8, OWN, the bank's voices; empty bank slots skipped as on the knobs); ANALOG: the
 *               Juno chorus, OFF / I / II / I+II (juno_chorus.c), as the buttons on a Juno */
#include "ncc_presets.c"                                /* (the Juno sounds: ncc_juno_load) */

#define LV_PARTS NCM_PARTS
#define LV_LOAD_MS 150u
#define LV_POP_MS 1000u
#define LV_HOT_MS 1500u                 /* a knob ring stays lit after its turn */

static const uint8_t LV_ENG[LV_PARTS] = {ENGI_FM6, ENGI_ANALOG};
static const char LV_NAME[LV_PARTS] = {'L', 'R'};

enum { PG_SOUND, PG_ARP, PG_FX, PG_N };
enum { LK_NONE, LK_TP, LK_G, LK_OCT, LK_AMODE, LK_ARATE, LK_ITYPE, LK_PERF };
typedef struct {
    uint8_t kind, id;                   /* LK_*, the P_* / G_* id (LK_PERF: perf_k's index) */
    const char *label;
} lv_knob_t;
/* KNOB 1..4 per page and its page 2 (SELECT); FX held: PERFORM's macros */
static const lv_knob_t LV_KNOBS[PG_N][2][4] = {
    [PG_SOUND] = {{{LK_TP, P_LEVEL, "LEVEL"}, {LK_OCT, 0, "OCTAVE"}, {LK_TP, P_VOICE, "VOICE"}, {LK_TP, P_GLIDE, "GLIDE"}},
                  {{LK_NONE, 0, ""}, {LK_NONE, 0, ""}, {LK_NONE, 0, ""}, {LK_NONE, 0, ""}}},
    [PG_ARP] = {{{LK_AMODE, P_AMODE, "MODE"}, {LK_ARATE, P_ARATE, "RATE"}, {LK_TP, P_AOCT, "OCTAVES"},
                 {LK_TP, P_AGATE, "GATE"}},
                {{LK_TP, P_ASWING, "SWING"}, {LK_TP, P_APROB, "PROB"}, {LK_TP, P_AORDER, "ORDER"}, {LK_NONE, 0, ""}}},
    [PG_FX] = {{{LK_TP, P_DIST, "DRIVE"}, {LK_TP, P_CHOR, "CHORUS"}, {LK_TP, P_DLY, "DELAY"}, {LK_TP, P_REV, "REVERB"}},
               {{LK_ITYPE, P_ITYPE, "INSERT"}, {LK_G, G_DTIME, "TIME"}, {LK_G, G_DFDBK, "FEEDBK"}, {LK_G, G_RSIZE, "SIZE"}}},
};
static const lv_knob_t LV_PERF[4] = {{LK_PERF, 0, "FILTER"}, {LK_PERF, 1, "CRUSH"}, {LK_PERF, 2, "THROW"},
                                     {LK_PERF, 3, "DEPTH"}};
static const uint8_t LV_RATES[] = {0, 1, 4, 2, 5, 3};   /* RATE on KNOB 2: 1/4 1/8 8T 1/16 16T 1/32 (N_DIV) */

static struct {
    uint8_t ok;                         /* lv_setup done */
    uint8_t page, sub;                  /* PG_*, its page 2 */
    uint8_t fx;                         /* FX held (its layer: the key map, the macros) */
    uint8_t hot;                        /* the knob turned last + 1 (0 none) */
    uint32_t hot_ms;                    /* .. when, fm1_ms | 1 */
    uint8_t amode[LV_PARTS];            /* the arp's mode while off (ARP on brings it back) */
    int16_t pend;                       /* PRESETS detents not loaded yet */
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
        if (i < LV_PARTS && t->eng_req != LV_ENG[i])
            set_engine_of(t, LV_ENG[i]);
        track_defaults_steps(t);                        /* the Circuit plays: no pattern of ours starts with its clock */
        if (i >= LV_PARTS)
            t->p[P_MUTE] = 1;
        if (i < LV_PARTS)
            lv.amode[i] = (uint8_t)(t->p[P_AMODE] ? t->p[P_AMODE] : 1);   /* (UP) */
    }
    if (song.g[G_ROUTE] != ROUTE_TRACK) {
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

/* PRESETS: d sounds on from the focused part's (on ANALOG the Juno sounds first, then eng_list_step's list: the
 * engine's presets, then its user presets) */
static void lv_list_go(int32_t d)
{
    uint32_t te, ce = eng_list_pos(&te), e = TSEL->eng_req % NENGINES, np = preset_shown(e), k, n, part = song.sel;
    uint32_t nj = e == ENGI_ANALOG && part < LV_PARTS ? NCC_NJUNO : 0u, total = nj + te;
    uint32_t cur = nj && lv_juno(part) ? lv.juno[part] - 1u : nj + ce;
    if (total < 2u)
        return;
    d %= (int32_t)total;
    n = (cur + (uint32_t)(d + (int32_t)total)) % total;
    if (n < nj) {
        ncc_juno_load(TSEL, n);
        lv.juno[part] = (uint8_t)(n + 1u);
        lv.jsig[part] = lv_snd(TSEL);
        return;
    }
    n -= nj;
    if (part < LV_PARTS)
        lv.juno[part] = 0;
    if (n < np) {
        for (k = 0; preset_orig(ENGINES[e], k) != k || n--; k++)   /* the n-th shown preset */
            ;
        apply_preset(k);
        return;
    }
    n -= np;
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && up_engine(k) == e && !n--) {
            up_load(k);
            break;
        }
}

static void lv_focus(uint32_t part)
{
    if (lv.pend) {                                      /* a sound picked on the other part: loaded there first */
        lv_list_go(lv.pend);
        lv.pend = 0;
    }
    track_select(part);
}

/* ---------------------------------------------------------- the knobs --- */
static const lv_knob_t *lv_knob(uint32_t k)
{
    return lv.fx ? &LV_PERF[k & 3u] : &LV_KNOBS[lv.page % PG_N][lv.sub & 1u][k & 3u];
}
static const param_desc_t *lv_desc(const lv_knob_t *kb)
{
    return kb->kind == LK_G ? &GP[kb->id] : param_desc_of(eng_idx(TSEL->eng_req), kb->id);
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
    if (hi - lo > 60)
        s *= 2;                                         /* (0..127 in two turns or so) */
    v = clamp(from + s, lo, hi);
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
    case LK_ITYPE:
        t->p[P_ITYPE] = (int16_t)v;
        if (v && !t->p[P_IMIX])
            t->p[P_IMIX] = 100;                         /* (a type with MIX 0 is not heard) */
        break;
    case LK_G: {
        const param_desc_t *d = lv_desc(kb);
        song.g[kb->id] = (int16_t)(d->names ? enum_step(d, from, v) : v);
        break;
    }
    default: {
        const param_desc_t *d = lv_desc(kb);
        t->p[kb->id] = (int16_t)(d->names ? enum_step(d, from, v) : v);
    }
    }
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
    } else if (lv.page == PG_ARP) {
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
    kb_mask = perf_mask = fxb;                          /* FX held: the keys are PERFORM's (seq.c keyboard_block) */
    if (lv.fx && !(btn & fxb) && !perf_latch_on)
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;   /* (the macros with the layer: off with it) */
    lv.fx = (btn & fxb) != 0u;
    ncm_throw = (uint8_t)(((btn >> panel.btn[B_ENV]) & 1u) | ((btn >> panel.btn[B_LFO]) & 1u) << 1);
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        lv_focus(0);
    if ((pressed >> panel.btn[B_OCTUP]) & 1u)
        lv_focus(1);
    if ((pressed >> panel.btn[B_HOME]) & 1u)
        lv.page = PG_SOUND, lv.sub = 0;
    if ((pressed >> panel.btn[B_FX]) & 1u)
        lv.page = PG_FX, lv.sub = 0;
    if ((pressed >> panel.btn[B_ARP]) & 1u)
        lv_arp_button();
    if ((pressed >> panel.btn[B_REC]) & 1u && song.sel < LV_PARTS) {
        TSEL->p[P_AHOLD] = (int16_t)!TSEL->p[P_AHOLD];
        lv_say_part("HOLD", TSEL->p[P_AHOLD]);
    }
    if ((pressed >> panel.btn[B_SEQ]) & 1u) {
        if ((btn >> panel.btn[B_GLO]) & 1u) {           /* GLO + SEQ: ST <-> SPLIT */
            song.g[G_ENGGO] = (int16_t)!NCC_SPLIT();
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
    if ((s = panel_enc(EN_ALGO)) != 0 && TSEL->eng_req == ENGI_FM6) {
        const param_desc_t *d = param_desc_of(ENGI_FM6, P_E7);
        int32_t from = TSEL->p[P_E7], v = clamp(from + s, d->min, d->max);
        TSEL->p[P_E7] = (int16_t)enum_step(d, from, v);   /* (fm6_poll loads it) */
    } else if (s && TSEL->eng_req == ENGI_ANALOG && song.sel < LV_PARTS) {   /* the Juno chorus, the drift kept */
        int32_t x = clamp(TSEL->p[P_ED_FX], 0, 63), m = clamp((x & 3) + s, JC_OFF, JC_I_II);
        TSEL->p[P_ED_FX] = (int16_t)((x & ~3) | m);
        lv_say(song.sel ? "R CHORUS " : "L CHORUS ", JC_NAME[m]);
    }
    if ((s = panel_enc(EN_SELECT)) != 0 && (lv.page == PG_ARP || lv.page == PG_FX))
        lv.sub = s > 0 ? 1u : 0u;                       /* page 2 / back */
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
    led_put(nl, panel.btn[B_FX], lv.page == PG_FX || perf_act != 0u);
    led_put(nb, panel.btn[B_ENV], (ncm_throw & 1u) != 0);
    led_put(nb, panel.btn[B_LFO], (ncm_throw & 2u) != 0);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = fm1_led_mid[c] = 0;
        fm1_led_breath[c] = nb[c];
        fm1_led[c] = nl[c] | nb[c];
    }
}

#include "live_draw.c"                                  /* the screen */
