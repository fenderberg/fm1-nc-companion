/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC settings and sound-edit adapters. Kept separate from Felucca's navigation so the panel never opens its
 * sequencer/song pages. Engine descriptors and FM6's existing patch writer supply the values and safe ranges.
 * Main loop only; all part edits use the focused part, including while Dual plays both. */
static const lv_knob_t LV_SETTINGS[2][4] = {
    {{LK_CHANNEL, 0, "L CH"}, {LK_CHANNEL, 1, "R CH"}, {LK_OUTPUT, 0, "OUTPUT"}, {LK_G, G_CLOCK, "CLOCK"}},
    {{LK_DUAL, 0, "DUAL"}, {LK_G, G_BPM, "BPM"}, {LK_DRONESTATE, 0, "L DRONE"}, {LK_DRONESTATE, 1, "R DRONE"}},
};
#define LV_PARAM(id) {LK_TP, id, ""}
enum { LE_ENGINE1, LE_ENGINE2, LE_AMP, LE_ENV, LE_LFO, LE_LDEST, LE_LSYNC, LE_VOICE, LE_ALLOC, LE_CHORD,
       LE_FILTER, LE_FOP, LE_FENV, LE_FOP2, LE_N };
static const lv_knob_t LV_EDIT[LE_N][4] = {
    {LV_PARAM(P_E0), LV_PARAM(P_E1), LV_PARAM(P_E2), LV_PARAM(P_E3)},
    {LV_PARAM(P_E4), LV_PARAM(P_E5), LV_PARAM(P_E6), LV_PARAM(P_E7)},
    {LV_PARAM(P_ATK), LV_PARAM(P_DEC), LV_PARAM(P_SUS), LV_PARAM(P_REL)},
    {LV_PARAM(P_ED_FLT), LV_PARAM(P_ED_PIT), LV_PARAM(P_ED_SHP), LV_PARAM(P_ESYNC)},
    {LV_PARAM(P_LRATE), LV_PARAM(P_LWAVE), LV_PARAM(P_LPHASE), LV_PARAM(P_LFADE)},
    {LV_PARAM(P_LD_PIT), LV_PARAM(P_LD_FLT), LV_PARAM(P_LD_SHP), LV_PARAM(P_LD_AMP)},
    {LV_PARAM(P_LSYNC), LV_PARAM(P_LTRIG), LV_PARAM(P_LPOL), LK_EMPTY},
    {LV_PARAM(P_VOICE), LV_PARAM(P_GLIDE), LV_PARAM(P_GLMODE), LV_PARAM(P_PRIO)},
    {LV_PARAM(P_ALLOC), LV_PARAM(P_DETUNE), LV_PARAM(P_PAN), LV_PARAM(P_SPRD)},
    {LV_PARAM(P_CHRD), LV_PARAM(P_VOIC), LV_PARAM(P_ROOT), LV_PARAM(P_SCALE)},
    {LV_PARAM(P_FTYPE), LV_PARAM(P_E4), LV_PARAM(P_E5), LV_PARAM(P_E7)},
    {{LK_FOP, FO_OP, "OP"}, {LK_FOP, FO_CRS, "RATIO"}, {LK_FOP, FO_FINE, "FINE"}, {LK_FOP, FO_LVL, "LEVEL"}},
    {{LK_FOP, FO_OP, "OP"}, {LK_FOP, FO_STG, "STAGE"}, {LK_FOP, FO_RATE, "RATE"}, {LK_FOP, FO_ELVL, "LEVEL"}},
    {{LK_FOP, FO_OP, "OP"}, {LK_FOP, FO_MODE, "MODE"}, {LK_FOP, FO_DTUN, "DTUN"}, {LK_FOP, FO_VEL, "VEL"}},
};
#undef LV_PARAM
static const char *const LV_EDIT_NAME[LE_N] = {
    "ENGINE 1", "ENGINE 2", "AMP ENV", "ENV DEST", "LFO", "LFO DEST", "LFO SYNC", "VOICE", "VOICE 2", "CHORD",
    "FILTER", "OPERATOR", "OP ENV", "OP MODE"
};
static int lv_edit_visible(uint32_t i)
{
    uint32_t e = TSEL->eng_req;
    if (i == LE_AMP || i == LE_ENV)
        return e != ENGI_FM6;                           /* FM6 uses its operators' envelopes */
    if (i == LE_FILTER)
        return e == ENGI_ANALOG;
    return i < LE_FOP || e == ENGI_FM6;
}
static uint32_t lv_edit_index(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < LE_N; i++)
        if (lv_edit_visible(i) && n++ == lv.sub)
            return i;
    return 0;
}
static uint32_t lv_nsub(uint32_t page)
{
    uint32_t i, n = 0;
    if (page == PG_GLO)
        return NELEM(LV_SETTINGS);
    if (page != PG_EDIT)
        return LV_NSUB[page % PG_N];
    for (i = 0; i < LE_N; i++)
        n += (uint32_t)lv_edit_visible(i);
    return n;
}
static void lv_open(uint32_t page)
{
    lv.pop_ms = 0;
    lv.say = 0;
    if (lv.page != page)
        lv.page = (uint8_t)page, lv.sub = 0;
    else
        lv.sub = (uint8_t)((lv.sub + 1u) % lv_nsub(page));
}
#define LV_PAGE_HOLD_MS 500u
static void lv_page_step(int32_t direction)
{
    lv.sub = (uint8_t)clamp((int32_t)lv.sub + direction, 0, (int32_t)lv_nsub(lv.page) - 1);
    lv.pop_ms = 0;
    lv.say = 0;
}
/* Decide short on release, long once at the threshold. No sleep, repeat or transport
 * request. A changed family/subpage/part cancels the pending gesture, so HOME and
 * other page buttons cannot leave a delayed action on the newly opened screen. */
static void lv_page_button(uint32_t pressed, uint32_t buttons)
{
    uint32_t mask = 1u << panel.btn[B_PLAY];
    if (pressed & mask) {
        lv.page_down = lv_nsub(lv.page) > 1u;
        lv.page_ms = fm1_ms;
        lv.page_family = lv.page;
        lv.page_sub = lv.sub;
        lv.page_part = song.sel;
    }
    if (!lv.page_down)
        return;
    if (lv.page_family != lv.page || lv.page_sub != lv.sub || lv.page_part != song.sel) {
        lv.page_down = 0;
        return;
    }
    if (lv.page_down == 1u && fm1_ms - lv.page_ms >= LV_PAGE_HOLD_MS) {
        lv_page_step(-1);
        lv.page_sub = lv.sub;
        lv.page_down = 2;                             /* release must not also move forward */
    }
    if (!(buttons & mask)) {
        if (lv.page_down == 1u)
            lv_page_step(1);
        lv.page_down = 0;
    }
}
static const char *lv_page_name(void)
{
    if (lv.page == PG_EDIT)
        return LV_EDIT_NAME[lv_edit_index()];
    return lv.sub ? "SETUP 2" : "SETUP";
}
static uint32_t lv_view_key(void)
{
    return lv.page | (uint32_t)lv.sub << 4 | (uint32_t)lv.fx << 12 | (uint32_t)TSEL->eng_req << 16;
}
static void lv_fop_text(uint32_t field, int32_t v, char *s)
{
    static const char *const STAGE[] = {"ATK", "DEC", "SUS", "REL"};
    if (field == FO_OP)
        fmt_int(s, v + 1);
    else if (field == FO_STG)
        str_cpy(s, STAGE[v & 3], 16);
    else if (field == FO_MODE)
        str_cpy(s, v ? "FIXED" : "RATIO", 16);
    else if (field == FO_DTUN)
        fmt_int(s, v - 7);
    else if (field == FO_CRS && fop_bytes(song.sel)[FP_MODE]) {
        static const char *const HZ[] = {"1Hz", "10Hz", "100Hz", "1kHz"};
        str_cpy(s, HZ[v & 3], 16);
    } else if (field == FO_CRS && !v)
        str_cpy(s, "0.50", 16);
    else
        fmt_int(s, v);
}
