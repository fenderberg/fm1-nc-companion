/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the editor protocol's NCC commands (96..100), for the NCC Manager (web/ncc/): the sound
 * library as NCC shows it (ncc_library.c: NCC's own sounds, the presets, FM6's voice bank, the user presets, each with
 * its type), what the two parts play, a sound on a part, and NCC's settings. Main loop (editor.c ed_handle), as the
 * other commands; every reply fits ed_out (NCC_LIB: at most 16 entries a request).
 *   96 NCC_INFO   -              -> "NCC", Felucca's version, NC_N, the type names, the engines NCC offers: count,
 *                                   then each: engine byte, name
 *   97 NCC_LIB    start v14, n   -> start v14, n, total v14, then each: kind (NLK_*), engine byte, type (NC_*), name
 *   98 NCC_LOAD   part, i v14    -> part, rc (0 loaded, 1 arguments): entry i of the library on part 0 (L) / 1 (R)
 *   99 NCC_PARTS  -              -> 2, then per part: entry v14 (-1: none), type filter, engine filter (LV_ENGS + 1,
 *                                   0 ALL), MIDI channel (0..15, 16 OMNI, 17 OFF), engine byte, the sound's name; then
 *                                   SPLIT (1) / STEREO (0), the clock (0 INT, 1 USB, 2 TRS), the BPM
 *  100 NCC_SET    op, args       -> op, rc: 0 part, ch (0..15, 16 OMNI, 17 OFF): its MIDI channel; 1 on: SPLIT (1) or
 *                                   STEREO (0); 2 src: the clock, 0 INT (the BPM), 1 USB, 2 TRS; 3 part, type, engine: its
 *                                   filters (the sound stays) */
enum { ED_NCC_INFO = 96, ED_NCC_LIB, ED_NCC_LOAD, ED_NCC_PARTS, ED_NCC_SET };

static void ed_v14(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    ed_b(u & 127u);
    ed_b(u >> 7);
}
/* the name part shows (live_draw.c lv_draw_pad's way) */
static void ed_ncc_name(uint32_t part, char *s)
{
    const track_t *t = &trk[part];
    int32_t bank = t->eng_req == ENGI_FM6 ? (int32_t)t->p[P_E7] - (int32_t)FM6_OWN - 1 : -1;
    uint32_t jn = lv_juno(part);
    if (bank >= 0 && bank < (int32_t)FM6_NBANK && (nl_bank_names(), nl.bname[bank][0]))
        str_cpy(s, nl.bname[bank], 16);
    else if (t->eng_req == ENGI_LOOP && t->p[P_E0] >= (int16_t)NLOOP && user_of(t) >= UP_SLOTS) {   /* a user loop */
        if (!lu_name((uint32_t)(t->p[P_E0] - (int16_t)NLOOP), s))
            str_cpy(s, "NO LOOP", 16);
    } else if (jn)
        str_cpy(s, NCC_SND[jn - 1u].name, 16);
    else
        sound_name(t, s);
}
static int ed_ncc_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i, rc = 1;
    char s[24];
    if (cmd < ED_NCC_INFO || cmd > ED_NCC_SET)
        return 0;
    switch (cmd) {
    case ED_NCC_INFO:
        ed_str("NCC", 8);
        ed_str(FELUCCA_VERSION, 16);
        ed_b(NC_N);
        for (i = 0; i < NC_N; i++)
            ed_str(NC_NAME[i], 8);
        ed_b(NELEM(LV_ENGS));
        for (i = 0; i < NELEM(LV_ENGS); i++) {
            ed_b(LV_ENGS[i]);
            ed_str(LV_ENG_NAME[i], 8);
        }
        break;
    case ED_NCC_LIB: {
        int32_t start = n == 3u ? ed_rv(a) : 0, cnt = n == 3u ? (int32_t)a[2] : 0;
        nl_build();
        start = clamp(start, 0, (int32_t)nl.n);
        cnt = clamp(cnt, 0, 16);
        if (start + cnt > (int32_t)nl.n)
            cnt = (int32_t)nl.n - start;
        ed_v14(start);
        ed_b((uint32_t)cnt);
        ed_v14((int32_t)nl.n);
        for (i = 0; i < (uint32_t)cnt; i++) {
            const nl_snd_t *e = &nl.s[(uint32_t)start + i];
            ed_b(e->kind);
            ed_b(e->eng);
            ed_b(e->cat);
            if (e->kind == NLK_NCC)
                str_cpy(s, NCC_SND[e->k].name, 16);
            else if (e->kind == NLK_BANK)
                str_cpy(s, nl.bname[e->k], 16);
            else if (e->kind == NLK_USER)
                up_name(e->k, s);
            else if (e->kind == NLK_ULOOP) {            /* a user slot's loop: the slot's name (eng_loop.c) */
                if (!lu_name(e->k, s))
                    str_cpy(s, "NO LOOP", 16);
            }
            else
                str_cpy(s, ENGINES[e->eng]->presets[e->k].name, 16);
            ed_str(s, 16);
        }
        break;
    }
    case ED_NCC_LOAD:
        if (n == 3u && a[0] < LV_PARTS) {
            uint32_t old = song.sel, k = (uint32_t)clamp(ed_rv(a + 1), 0, 8191);
            nl_build();
            if (k < nl.n) {                             /* (on the part as if it had the focus: nl_load's TSEL) */
                song.sel = (uint8_t)a[0];
                nl_load(k);
                lv.juno[a[0]] = (uint8_t)(nl.s[k].kind == NLK_NCC ? nl.s[k].k + 1u : 0u);
                lv.jsig[a[0]] = lv_snd(&trk[a[0]]);
                song.sel = (uint8_t)old;
                rc = 0;
            }
        }
        ed_b(n ? a[0] : 0u);
        ed_b(rc);
        break;
    case ED_NCC_PARTS:
        nl_build();
        ed_b(LV_PARTS);
        for (i = 0; i < LV_PARTS; i++) {
            uint32_t ch = mch_get(i);
            ed_v14(nl_find(i, lv_juno(i)));
            ed_b(lv.cat[i]);
            ed_b(lv.ep[i]);
            ed_b(ch < 16u ? ch : ch == MCH_OMNI ? 16u : 17u);
            ed_b(eng_idx(trk[i].eng_req));
            ed_ncc_name(i, s);
            ed_str(s, 16);
        }
        ed_b((uint32_t)NCC_SPLIT());
        ed_b((uint32_t)clamp(song.g[G_CLOCK], 0, 2));
        ed_b((uint32_t)clamp(song.g[G_BPM], 0, 127) & 127u);
        ed_b((uint32_t)clamp(song.g[G_BPM], 0, 300) >> 7);
        break;
    case ED_NCC_SET:
        if (n == 3u && a[0] == 0u && a[1] < LV_PARTS && a[2] <= 17u) {
            mch_set(a[1], a[2] < 16u ? a[2] : a[2] == 16u ? MCH_OMNI : MCH_OFF);
            rc = 0;
        } else if (n == 2u && a[0] == 1u && a[1] <= 1u) {
            song.g[G_ENGGO] = 0;                        /* (core.h NCC_SPLIT: which value is SPLIT) */
            if (NCC_SPLIT() != (a[1] != 0u))
                song.g[G_ENGGO] = 1;
            rc = 0;
        } else if (n == 2u && a[0] == 2u && a[1] <= 2u) {
            song.g[G_CLOCK] = (int16_t)a[1];
            rc = 0;
        } else if (n == 4u && a[0] == 3u && a[1] < LV_PARTS && a[2] < NC_N && a[3] <= NELEM(LV_ENGS)) {
            lv.cat[a[1]] = a[2];
            lv.ep[a[1]] = a[3];
            rc = 0;
        }
        ed_b(n ? a[0] : 0u);
        ed_b(rc);
        break;
    }
    return 1;
}
