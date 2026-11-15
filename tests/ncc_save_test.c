/* SPDX-License-Identifier: GPL-3.0-only */
/* SAVE on the panel (ncc_save.c): asks first; a tap overwrites a user sound, held a new one named after its source,
 * in its category and under USER; written while the Circuit plays too. Host RAM store (up_put 0 or 3). */
static int save_said(const char *s) { return lv.say && !strncmp(lv.say, s, strlen(s)); }
static void save_test(void)
{
    up_bank_t saved[UP_SLOTS / UP_PER_BANK];
    uint32_t i, pad = 0, mask = 1u << panel.btn[B_SAVE], k, users;
    char base[16], want[16], b[16];
    int16_t rel;
    memcpy(saved, up_bank, sizeof saved);
    memset(up_bank, 0, sizeof up_bank);
    power(); lv_focus(1); nl_build();
    for (i = 0; i < nl.n && !(nl.s[i].cat == NC_PAD && nl.s[i].kind != NLK_USER && nl.s[i].eng == ENGI_ANALOG); i++)
        ;
    pad = i;
    nl_name(pad, base);
    nl_load(pad);                                      /* (as the panel loads it: lv_load_nth) */
    lv.juno[1] = (uint8_t)(nl.s[pad].kind == NLK_NCC ? nl.s[pad].k + 1u : 0u);
    lv.jsig[1] = lv_snd(&trk[1]);
    lv_frames(100);
    ok("save: a JUNO pad on R to start from", pad < nl.n && trk[1].eng_req == ENGI_ANALOG && user_of(&trk[1]) == UP_SLOTS);

    lv_press(B_SAVE);
    ok("save: one SAVE asks, a factory sound can only be new", ns.ask && save_said("HOLD SAVE: NEW SOUND") &&
       !up_count());
    lv_frames(2000);
    ok("save: the question stays on the foot while it is open", ns.ask && lv.pop_ms && lv.say_n == ns.shown);
    lv_press(B_SAVE);
    ok("save: a tap on a factory sound writes nothing and asks again", ns.ask && !up_count() &&
       save_said("HOLD SAVE: NEW SOUND"));
    page_hold(mask, 700);
    up_name(0, b);
    str_cpy(want, base, 11u);
    for (k = 0; want[k]; k++) want[k] = (char)nl_up((uint8_t)want[k]);
    ok("save: held: a new sound in the first slot, the part plays it", up_used(0) && up_count() == 1u &&
       user_of(&trk[1]) == 0u && !ns.ask && save_said("SAVED: "));
    ok("save: .. named after its source with a 2", !strncmp(b, want, strlen(want)) && b[strlen(b) - 1u] == '2' &&
       b[strlen(b) - 2u] == ' ');
    nl_build();
    for (i = 0, users = 0; i < nl.n; i++)
        if (nl.s[i].kind == NLK_USER) {
            users++;
            k = i;
        }
    ok("save: .. in its category (PAD) and under USER", up_cat(0) == CAT_PAD && users == 1u && nl.s[k].cat == NC_PAD &&
       nl_fits(k, NC_USER, 0) && nl_fits(k, NC_PAD, 0) && nl_count(NC_USER, 0) == 1u && !nl_fits(pad, NC_USER, 0));

    rel = trk[1].p[P_REL] = (int16_t)(trk[1].p[P_REL] > 60 ? 20 : 100);
    lv_press(B_SAVE);
    ok("save: on a user sound SAVE offers the overwrite", ns.ask && save_said("OVERWRITE? HOLD: NEW"));
    lv_press(B_SAVE);
    {
        int16_t v[P_COUNT];
        char b2[16];
        up_values(up_rec(0), v);
        up_name(0, b2);
        ok("save: a tap overwrites it: same slot, name and category, the new values", up_count() == 1u &&
           v[P_REL] == rel && str_eq(b2, b) && up_cat(0) == CAT_PAD && user_of(&trk[1]) == 0u);
    }
    lv_press(B_SAVE); page_hold(mask, 700);
    up_name(1, want);
    ok("save: held on a user sound: a copy, the next number", up_count() == 2u && user_of(&trk[1]) == 1u &&
       want[strlen(want) - 1u] == '3' && up_cat(1) == CAT_PAD);

    lv_press(B_SAVE); lv_press(B_HOME);
    ok("save: another button cancels it", !ns.ask && up_count() == 2u);
    lv_press(B_SAVE); lv_frames(NS_ASK_MS + 100u);
    ok("save: left open, it cancels itself and says so", !ns.ask && save_said("SAVE CANCELLED") && up_count() == 2u);

    song.playing = 1;
    lv_press(B_SAVE); page_hold(mask, 700);
    ok("save: written while the Circuit plays", up_count() == 3u && song.playing);
    song.playing = 0;

    lv_focus(0);                                       /* L's FM6 sound: its patch kept beside the record */
    lv_press(B_SAVE); page_hold(mask, 700);
    k = user_of(&trk[0]);
    ok("save: an FM6 sound saved, loaded back", k < UP_SLOTS && up_engine(k) == ENGI_FM6 && !up_load(k) &&
       trk[0].eng_req == ENGI_FM6);

    for (i = 0; i < UP_SLOTS; i++)
        if (!up_used(i)) {
            up_rec_t *r = up_rec(i);
            memset(r, 0, sizeof *r);
            r->used = UP_USED; r->ver = UP_VER; r->engine = ENGI_ANALOG; r->np = P_COUNT; memcpy(r->name, "FILL", 4);
        }
    lv_press(B_SAVE); page_hold(mask, 700);
    ok("save: a full bank says so", up_count() == UP_SLOTS && save_said("USER BANK FULL"));

    memset(up_bank, 0, sizeof up_bank);
    ns_new_name(b, "ACID 303");
    ok("save names: a model number stays (ACID 303 2)", str_eq(b, "ACID 303 2"));
    ns_new_name(b, "Soft Pad 7");
    ok("save names: a copy's number goes, upper case (SOFT PAD 2)", str_eq(b, "SOFT PAD 2"));
    ns_new_name(b, "ABCDEFGHIJKLMN");
    ok("save names: cut to fit twelve", str_eq(b, "ABCDEFGHIJ 2"));
    ns_new_name(b, "");
    ok("save names: none: SOUND 2", str_eq(b, "SOUND 2"));
    ok("save screen: the questions and a saved name fit the foot", text_w(&AF_M, "OVERWRITE? HOLD: NEW") <= 236 &&
       text_w(&AF_M, "HOLD SAVE: NEW SOUND") <= 236 && text_w(&AF_M, "SAVED: MMMMMMMMMMMM") <= 236 &&
       text_w(&AF_S, "USER") <= 40);
    memcpy(up_bank, saved, sizeof saved);
    power();
}
