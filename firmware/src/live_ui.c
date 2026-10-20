/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (Novation Circuit Companion, FELUCCA_LIVEUI=1): the live UI, milestone 1. main.c calls live_input / live_leds /
 * live_draw in place of ui_input / ui_leds / ui_draw; the rest of the UI stays built (sound loads, user presets, the
 * settings and projects, MIDI Program Change, the update countdown's screen) and is called from here.
 *   two parts   L = track 1, FM6; R = track 2, ANALOG; tracks 3 and 4 muted and deaf. No sequencer: the Circuit
 *               Tracks plays them, MIDI IN TRACK, L on channel 3 and R on 4 (its MIDI 1 / MIDI 2 tracks) unless the
 *               project has TRACK already (then its channels, MENU > MIDI in the web editor)
 *   OCT- / OCT+ the focus: L / R (its LED: OCT+ has none in PANEL_DEFAULT, so R is OCT- dark); the 27 keys play the
 *               part in focus (song.sel)
 *   PRESETS     the focused part's sounds (its engine's presets, then its user presets), loaded once the knob has
 *               rested LV_LOAD_MS, so turning through the list loads one sound, not each one passed
 *   ALGORITHM   FM6: SLOT (F1..F8, OWN, the bank's voices; empty bank slots skipped as on the knobs)
 *   the screen  a head (NCC, the tempo: amber on an external clock), a column per part (channel, engine, sound,
 *               SLOT, a MIDI dot; the focused one lit, the other dim), a foot: the Circuit knob just turned (its
 *               legend and value, LV_POP_MS) or the controls. Only what changed is drawn (lv.sig) */

#define LV_PARTS NCM_PARTS
#define LV_LOAD_MS 150u
#define LV_POP_MS 1000u
#define LV_W 120u                       /* a column */
#define LV_Y_COL 34u
#define LV_Y_FOOT 204u

static const uint8_t LV_ENG[LV_PARTS] = {ENGI_FM6, ENGI_ANALOG};
static const char LV_NAME[LV_PARTS] = {'L', 'R'};

static struct {
    uint8_t ok;                         /* lv_setup done */
    int16_t pend;                       /* PRESETS detents not loaded yet */
    uint32_t pend_ms;                   /* the last of them, fm1_ms | 1 */
    uint32_t seen;                      /* the ncm_seen the foot shows */
    uint32_t pop_ms;                    /* the foot's popup since (fm1_ms | 1), 0 none */
    uint32_t sig[LV_PARTS + 2u];        /* what is on the screen: the columns, the head, the foot (0: redraw) */
} lv;

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
    }
    if (song.g[G_ROUTE] != ROUTE_TRACK) {
        song.g[G_ROUTE] = ROUTE_TRACK;
        mch_set(0, 2u);                                 /* channel 3: the Circuit's MIDI 1 */
        mch_set(1, 3u);                                 /* channel 4: its MIDI 2 */
        mch_set(2, MCH_OFF);
        mch_set(3, MCH_OFF);
    }
    if (song.sel >= LV_PARTS)
        track_select(0);
    lv.ok = 1;
}

/* PRESETS: d sounds on from the focused part's (eng_list_step's list: the engine's presets, then its user presets) */
static void lv_list_go(int32_t d)
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES, np = preset_shown(e), k, n;
    if (total < 2u)
        return;
    d %= (int32_t)total;
    n = (cur + (uint32_t)(d + (int32_t)total)) % total;
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

static void live_input(void)
{
    uint32_t pressed = fm1_input_edges(0), k;
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
    if ((pressed >> panel.btn[B_OCTDN]) & 1u)
        lv_focus(0);
    if ((pressed >> panel.btn[B_OCTUP]) & 1u)
        lv_focus(1);
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
    }
    for (k = 0; k < NE; k++)                            /* the other knobs: nothing yet (no turn kept for later) */
        if (k != EN_PRESET && k != EN_ALGO)
            (void)panel_enc(k);
}

static void live_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0};
    uint32_t c;
    static uint8_t ready;
    if (fm1_led_anim_on())                              /* (the power-on sweep has them) */
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[song.sel ? B_OCTUP : B_OCTDN], 1);   /* (PANEL_DEFAULT knows no LED of OCT+: R shows as OCT- dark) */
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = fm1_led_breath[c] = fm1_led_mid[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* ------------------------------------------------------------- screen --- */
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
static uint32_t lv_bpm(int *ext)
{
    uint32_t b = beat_samples();
    *ext = song.g[G_CLOCK] && midi_beat_samples;
    return b ? ((uint32_t)FS * 60u + b / 2u) / b : 0u;
}

static void lv_draw_head(void)
{
    char s[12];
    int ext;
    uint32_t bpm = lv_bpm(&ext), sig = bpm << 1 | (uint32_t)ext | 1u << 31;
    if (sig == lv.sig[LV_PARTS])
        return;
    lv.sig[LV_PARTS] = sig;
    lv_num(s, bpm);
    str_cpy(s + str_len(s), " BPM", 5);
    draw_text_line(8, 4, 112, &AF_M, "NCC", T_THEME, T_BG, 0);
    draw_text_line(120, 4, 112, &AF_M, s, ext ? T_ACCENT : T_MID, T_BG, 2);
    lcd_fill(0, 26, 240, 1, T_LINE);
}

static void lv_draw_col(uint32_t part)
{
    const track_t *t = &trk[part];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t x = part * LV_W + 8u, on = song.sel == part, lit = (uint32_t)midi_lit(part), ch = mch_get(part), sig;
    uint16_t fg = on ? T_TEXT : T_DIM;
    char s[20];
    sig = (t->eng_req | (uint32_t)t->preset << 4 | (uint32_t)t->user << 12 | (uint32_t)(t->p[P_E7] & 63) << 18 |
           on << 24 | lit << 25 | (uint32_t)(on && lv.pend) << 26 | (ch & 31u) << 27) ^ 0x80000000u;
    if (sig == lv.sig[part])
        return;
    lv.sig[part] = sig;
    s[0] = LV_NAME[part];
    s[1] = ' ';
    if (ch < 16u) {
        str_cpy(s + 2, "CH ", 4);
        lv_num(s + 5, ch + 1u);
    } else
        str_cpy(s + 2, ch == MCH_OMNI ? "OMNI" : "OFF", 5);
    draw_text_line(x, LV_Y_COL, 92, &AF_S, s, on ? T_THEME : T_DIM, T_BG, 0);
    lcd_fill(x + 96u, LV_Y_COL + 3u, 8, 8, lit ? T_ACCENT : T_LINE);    /* MIDI IN on the part */
    draw_text_line(x, LV_Y_COL + 20u, 104, &AF_S, e->name, on ? T_MID : T_DIM, T_BG, 0);
    sound_name(t, s);
    draw_text_line(x, LV_Y_COL + 40u, 104, &AF_M, s, fg, T_BG, 0);
    s[0] = 0;
    if (on && lv.pend)
        str_cpy(s, lv.pend > 0 ? "NEXT >" : "< PREV", 7);
    else if (t->eng_req == ENGI_FM6)
        str_cpy(s, N_FM6_PATCH[clamp(t->p[P_E7], 0, (int32_t)(FM6_OWN + FM6_NBANK))], 5);
    draw_text_line(x, LV_Y_COL + 64u, 104, &AF_S, s, on ? T_MID : T_DIM, T_BG, 0);
    lcd_fill(x, LV_Y_COL + 86u, 104, 3, on ? T_ACCENT : T_BG);         /* the focus bar */
}

static void lv_draw_foot(void)
{
    uint32_t seen = ncm_seen, sig;
    char s[24];
    if (seen != lv.seen) {
        lv.seen = seen;
        lv.pop_ms = fm1_ms | 1u;
    }
    if (lv.pop_ms && (fm1_ms | 1u) - lv.pop_ms >= LV_POP_MS)
        lv.pop_ms = 0;
    sig = lv.pop_ms ? (seen & 0xFFFFFFu) | 1u << 30 : 1u << 31;
    if (sig == lv.sig[LV_PARTS + 1u])
        return;
    lv.sig[LV_PARTS + 1u] = sig;
    lcd_fill(0, LV_Y_FOOT - 6u, 240, 1, T_LINE);
    if (!lv.pop_ms) {                                   /* (the popup's taller line away first) */
        lcd_fill(0, LV_Y_FOOT, 240, AF_M.h, T_BG);
        draw_text_line(8, LV_Y_FOOT, 224, &AF_S, "OCT- L   OCT+ R   PRESETS: SOUND", T_MID, T_BG, 0);
        return;
    }
    s[0] = LV_NAME[(seen >> 16) & 1u];
    s[1] = ' ';
    str_cpy(s + 2, NCM_NAME[(seen >> 8) & 7u], 20);
    draw_text_line(8, LV_Y_FOOT, 168, &AF_M, s, T_TEXT, T_BG, 0);
    lv_num(s, seen & 127u);
    draw_text_line(176, LV_Y_FOOT, 56, &AF_M, s, T_THEME, T_BG, 2);
}

static void live_draw(void)
{
    uint32_t i;
    style_apply();
    ui.frame++;
    if (ui.uboot) {                                     /* OCT- + OCT+ held (main.c): UPDATE MODE's countdown */
        draw_uboot();
        memset(lv.sig, 0, sizeof lv.sig);
        return;
    }
    if (ui.force) {                                     /* (after the countdown, an update, a palette) */
        ui.force = 0;
        lcd_fill(0, 0, 240, 240, T_BG);
        memset(lv.sig, 0, sizeof lv.sig);
        lcd_fill(LV_W - 1u, LV_Y_COL, 2, 90, T_LINE);
    }
    lv_draw_head();
    for (i = 0; i < LV_PARTS; i++)
        lv_draw_col(i);
    lv_draw_foot();
}
