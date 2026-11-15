/* SPDX-License-Identifier: GPL-3.0-only */
/* Extended NCC panel and chord gestures: physical releases, not time windows or sustain, delimit REPLACE chords. */
static void page_hold(uint32_t mask, uint32_t ms)       /* a page button held ms, then let go */
{
    fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
    lv_frames(ms); fm1_in.buttons &= ~mask; lv_frame();
}
static void page_button_test(void)
{
    static const uint8_t BTN[] = {B_FX, B_GLO, B_EDIT}, PAGE[] = {PG_FX, PG_GLO, PG_EDIT};
    uint32_t i, mask, play = 1u << panel.btn[B_PLAY], playing;
    track_t *t = &trk[0];
    for (i = 0; i < NELEM(BTN); i++) {
        mask = 1u << panel.btn[BTN[i]];
        power(); playing = song.playing;
        fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
        ok("pages: a page button opens its first page at once, from another page", lv.page == PAGE[i] && !lv.sub);
        lv_frames(700); fm1_in.buttons &= ~mask; lv_frame();
        ok("pages: .. and that press held long does not also go back or on", lv.page == PAGE[i] && !lv.sub);
        t_turn(EN_SELECT, 1);
        ok("pages: SELECT on any page: the category coming, the page as it was", lv.page == PAGE[i] && !lv.sub &&
           lv.cpend == 1);
        lv_frames(LV_ENG_MS + 100u);
        ok("pages: .. then the category, still on the page", lv.page == PAGE[i] && !lv.sub && !lv.cpend &&
           lv.cat[0] != NC_ALL);
        fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
        ok("pages: on its own page the press waits for short/long", !lv.sub);
        fm1_in.buttons &= ~mask; lv_frame();
        ok("pages: a tap advances one page without changing transport", lv.sub == 1u && song.playing == playing &&
           !transport_req);
        fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
        fm1_ms += LV_PAGE_HOLD_MS - 32u; lv_frame();
        ok("pages: held below 500 ms does not move", lv.sub == 1u);
        lv_frame();
        ok("pages: at 500 ms it goes back once", !lv.sub);
        lv_frames(700);
        fm1_in.buttons &= ~mask; lv_frame();
        ok("pages: held long neither repeats nor advances on release", !lv.sub && !lv.page_down && lv.page == PAGE[i]);
        page_hold(mask, 700);
        ok("pages: held on the first page: the last", lv.page == PAGE[i] && lv.sub + 1u == lv_nsub(PAGE[i]));
        lv_press(BTN[i]);
        ok("pages: a tap on the last page: the first", lv.page == PAGE[i] && !lv.sub);
        /* A delayed release also counts as long if no input pass ran at the threshold. */
        lv_press(BTN[i]);
        fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
        fm1_ms += LV_PAGE_HOLD_MS;
        fm1_in.buttons &= ~mask; lv_frame();
        ok("pages: a long release without an intermediate input pass still goes backwards", !lv.sub);
        lv_press(BTN[i]);
        fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
        lv_press(B_HOME);
        fm1_in.buttons &= ~mask; lv_frame();
        ok("pages: HOME cancels a pending page gesture", lv.page == PG_SOUND && !lv.sub && !lv.page_down);
        lv_press(BTN[i]); lv_press(BTN[i]);
        page_hold(play, 700); lv_press(B_PLAY);
        ok("pages: PLAY does nothing on a page, including transport", lv.page == PAGE[i] && lv.sub == 1u &&
           song.playing == playing && !transport_req);
    }
    power(); playing = song.playing;
    page_hold(play, 700); lv_press(B_PLAY);
    ok("pages: PLAY does nothing on SOUND, including transport", lv.page == PG_SOUND && !lv.sub && song.playing == playing && !transport_req);
    lv_press(B_EDIT);
    for (i = 0; i + 1u < lv_nsub(PG_EDIT); i++) lv_press(B_EDIT);
    ok("pages: EDIT taps reach the last visible edit page", lv.page == PG_EDIT && lv.sub + 1u == lv_nsub(PG_EDIT));
    lv_press(B_EDIT);
    ok("pages: the tap after the last edit page: the first again", lv.page == PG_EDIT && !lv.sub);
    for (i = 0; i + 1u < lv_nsub(PG_EDIT); i++) lv_press(B_EDIT);
    mask = 1u << panel.btn[B_EDIT];
    fm1_ms = 0xFFFFFF00u;
    page_hold(mask, 600);
    ok("pages: held EDIT also works across the millisecond counter wrap", lv.sub + 2u == lv_nsub(PG_EDIT));
    lv_press(B_GLO); lv_press(B_GLO); lv_press(B_GLO);
    ok("pages: the tap after GLO's last page: the first again", lv.page == PG_GLO && !lv.sub);

    power(); lv_press(B_GLO);                          /* GLO + SEQ: SPLIT, no page step with it */
    i = (uint32_t)NCC_SPLIT();
    mask = 1u << panel.btn[B_GLO];
    fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
    lv_press(B_SEQ); lv_frames(700);
    fm1_in.buttons &= ~mask; lv_frame();
    ok("pages: GLO held + SEQ switches SPLIT and neither goes on nor back", (uint32_t)NCC_SPLIT() != i &&
       lv.page == PG_GLO && !lv.sub);
    fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
    lv_press(B_SEQ);
    fm1_in.buttons &= ~mask; lv_frame();
    ok("pages: .. a quick GLO + SEQ neither", (uint32_t)NCC_SPLIT() == i && !lv.sub);

    power(); lv_press(B_FX); lv_press(B_FX);           /* FX held + a key: PERFORM, no page step with it */
    mask = 1u << panel.btn[B_FX];
    fm1_in.buttons |= mask; host_pressed |= mask; lv_frame();
    key_down(white_key(0)); lv_frame(); lv_frames(700); key_up(white_key(0));
    fm1_in.buttons &= ~mask; lv_frame();
    ok("pages: FX held + a key plays PERFORM and leaves the page", lv.fx && lv.sub == 1u);
    for (i = 0; i < 4u; i++) lv_press(B_FX);
    ok("pages: FX taps reach PERFORM", lv.fx && lv.sub == LV_FX_PERF);
    page_hold(mask, 700);
    ok("pages: FX held on PERFORM goes back to INSERT", lv.fx && lv.sub == LV_FX_PERF - 1u);
    lv_press(B_FX); lv_press(B_FX);
    ok("pages: the tap after PERFORM: SENDS again, FX stays open", lv.fx && !lv.sub);

    power(); lv_press(B_ARP);                          /* ARP: the arp's on/off; KNOB 4 its pages */
    ok("pages: ARP opens its first page, KNOB 4 shows 1/3", lv.page == PG_ARP && !lv.sub &&
       !strcmp(lv_label(lv_knob(3)), "PAGE"));
    t_turn(EN_K4, 1);
    ok("pages: ARP: KNOB 4 right the next page, the arp stays on", lv.sub == 1u && t->p[P_AMODE]);
    t_turn(EN_K4, 5);
    ok("pages: ARP: KNOB 4 stops at the last page", lv.sub == 2u);
    t_turn(EN_K4, -1);
    ok("pages: ARP: KNOB 4 left the page before", lv.sub == 1u);
    t_turn(EN_K4, -5);
    ok("pages: ARP: KNOB 4 stops at the first page", !lv.sub);
    page_hold(play, 700); lv_press(B_PLAY);
    ok("pages: PLAY does nothing on ARP", lv.page == PG_ARP && !lv.sub && t->p[P_AMODE] && !transport_req);
    lv_press(B_ARP);
    ok("pages: ARP on its page still switches the arp off", !t->p[P_AMODE]);
}

static void replace_chord_test(void)
{
    track_t *t;
    uint32_t i;
    power(); t = &trk[1]; t->p[P_VOICE] = V_POLY; t->p[P_REL] = 127;
    ncd_set_mode(1, 1); ncd_want[1] = 1;
    midi(0x93, 60, 100); midi(0x93, 64, 90); midi(0x93, 67, 80); blk();
    ok("REPLACE chord: all tones wait for one fade together", ncd[1].pending == 7u && !gate_on(t));
    perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: a simultaneous triad rings as a chord", ncd[1].n == 3u && note_playing(t, 60) &&
       note_playing(t, 64) && note_playing(t, 67));
    midi(0x83, 60, 0); midi(0x83, 64, 0); midi(0x83, 67, 0); blk();
    ok("REPLACE chord: all keys released leaves the triad droning", !ncc_midi_down[1] && ncd[1].n == 3u && gate_on(t));
    midi(0x93, 62, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: next gesture fades the whole old triad", ncd[1].n == 1u && note_playing(t, 62) &&
       !note_playing(t, 60) && !note_playing(t, 64) && !note_playing(t, 67));
    perf_blocks(200);                                  /* a slow addition is still the same held gesture */
    midi(0x93, 65, 100); midi(0x93, 69, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: held keys allow adding tones without a time limit", ncd[1].n == 3u &&
       note_playing(t, 62) && note_playing(t, 65) && note_playing(t, 69));
    midi(0x83, 62, 0); midi(0x83, 65, 0); blk();
    midi(0x93, 72, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: one held key keeps the gesture open", ncd[1].n == 4u && note_playing(t, 62) && note_playing(t, 72));
    midi(0x83, 69, 0); midi(0x83, 72, 0); blk();
    midi(0xB3, 64, 127); midi(0x93, 55, 100); perf_blocks(NCD_FADE_BLOCKS + 3u); midi(0x83, 55, 0); blk();
    ok("REPLACE chord: sustain is not a physically held key", !ncc_midi_down[1] && midi_owners[1]);
    midi(0x93, 57, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: a new chord replaces a pedal-held chord", ncd[1].n == 1u &&
       note_playing(t, 57) && !note_playing(t, 55));
    midi(0xB3, 120, 0); blk();
    ok("REPLACE chord: panic forgets physical roots and pending tones", !ncc_midi_down[1] && !ncd[1].pending && !ncd[1].n);

    power(); t = &trk[0]; t->p[P_VOICE] = V_POLY; t->p[P_CHRD] = CH_MAJ;
    ncd_set_mode(0, 1); ncd_want[0] = 1;
    midi(0x92, 60, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: one CHORD root expands as one gesture", ncd[0].n == 3u &&
       note_playing(t, 60) && note_playing(t, 64) && note_playing(t, 67));
    midi(0x82, 60, 0); blk(); midi(0x92, 62, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: the next mapped chord replaces all old tones", ncd[0].n == 3u &&
       !note_playing(t, 60) && note_playing(t, 62) && note_playing(t, 66) && note_playing(t, 69));

    power(); ncc_set_dual(1); ncd_set_mode(1, 1); ncd_want[1] = 1;
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY; blk();
    key_down(10); key_down(14); key_down(17); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: local Dual keys hold a whole R chord", ncd[1].n == 3u && gate_on(&trk[0]) && gate_on(&trk[1]));
    key_up(10); key_up(14); key_up(17); blk();
    key_down(12); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: release all local keys then strike replaces the chord", ncd[1].n == 1u &&
       note_playing(&trk[1], kb_note[12]));
    key_up(12); blk();
    midi(0x92, 72, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE chord: MIDI after released local keys starts a new gesture", ncd[1].n == 1u && note_playing(&trk[1], 72));
    for (i = 0; i < 20u; i++) {
        midi(0x92, 72, 100); midi(0x82, 72, 0); blk();
    }
    ok("REPLACE chord: retriggers and duplicate offs cannot underflow physical count", !ncc_midi_down[0] && !ncc_midi_down[1]);
}

static void settings_edit_test(void)
{
    int16_t before, other;
    uint32_t e, p, k, fits = 1;
    power();
    lv_press(B_GLO);
    ok("settings UI: GLO opens SETUP", lv.page == PG_GLO && lv.sub == 0u && led_is(B_GLO, 1));
    t_turn(EN_K1, 1); t_turn(EN_K2, 1);
    ok("settings UI: both input channels can be changed on the device", mch_get(0) == 3u && mch_get(1) == 4u);
    t_turn(EN_K3, -1);
    ok("settings UI: output can be set to SPLIT", NCC_SPLIT());
    t_turn(EN_K4, 1);
    ok("settings UI: clock can select TRS", song.g[G_CLOCK] == 2);
    lv_press(B_GLO); t_turn(EN_K1, 2); t_turn(EN_K2, 5); t_turn(EN_K3, 2); t_turn(EN_K4, 1);
    ok("settings UI: page 2 sets Dual, BPM and both Drone modes", ncc_dual_mode() == 2u && song.g[G_BPM] == 130 &&
       ncd_mode(0) && ncd_want[0] && !ncd_mode(1) && ncd_want[1]);
    dual_button();
    ok("settings UI: SCL turns Dual off without leaving settings", !ncc_dual_mode() && lv.page == PG_GLO);
    dual_button();
    ok("settings UI: SCL remembers CH4", ncc_dual_mode() == 2u);
    fm1_in.buttons |= 1u << panel.btn[B_GLO]; lv_press(B_HOME); fm1_in.buttons = 0;
    ok("settings UI: HOME returns to SOUND, including with GLO held", lv.page == PG_SOUND && ncc_dual_mode() == 2u);

    power(); lv_focus(1); lv_press(B_EDIT);
    ok("edit UI: EDIT opens the selected part's engine controls", lv.page == PG_EDIT && lv_edit_index() == LE_ENGINE1);
    lv_press(B_EDIT); before = trk[1].p[P_E4]; other = trk[0].p[P_E4];
    trk[1].p[P_DLY] = 53; trk[1].p[P_REV] = 54;
    t_turn(EN_K1, -5);
    ok("edit UI: cutoff edits only focused R and keeps adjusted sends", trk[1].p[P_E4] < before &&
       trk[0].p[P_E4] == other && trk[1].p[P_DLY] == 53 && trk[1].p[P_REV] == 54);
    undo_swap();
    ok("edit UI: parameter edits support the existing undo snapshot", trk[1].p[P_E4] == before);
    lv_press(B_HOME); lv_press(B_EDIT); lv_press(B_EDIT); lv_press(B_EDIT);
    before = trk[1].p[P_ATK]; t_turn(EN_K1, 3);
    ok("edit UI: AMP ENV edits attack", lv_edit_index() == LE_AMP && trk[1].p[P_ATK] > before);
    lv_focus(0); lv.page = PG_EDIT;
    for (p = 0; p < lv_nsub(PG_EDIT); p++) {
        lv.sub = (uint8_t)p;
        if (lv_edit_index() == LE_FOP)
            break;
    }
    other = fm6_patch[1][fop_byte(FO_LVL)]; before = fm6_patch[0][fop_byte(FO_LVL)];
    trk[0].p[P_DLY] = 47;
    t_turn(EN_K4, before > 50 ? -2 : 2);
    ok("edit UI: FM6 operator edits use the patch writer on L only", fm6_patch[0][fop_byte(FO_LVL)] != before &&
       fm6_patch[1][fop_byte(FO_LVL)] == other && trk[0].p[P_E7] == FM6_OWN && trk[0].p[P_DLY] == 47);
    lv.sub = 1; fm6_bank_used = 0; t_turn(EN_K4, 1);
    ok("edit UI: an unavailable FM6 slot preserves the current sound and sends", trk[0].p[P_E7] == FM6_OWN &&
       trk[0].p[P_DLY] == 47);
    for (e = 0; e < NELEM(LV_ENGS); e++) {
        set_engine_of(&trk[0], LV_ENGS[e]); lv_focus(0); lv.page = PG_EDIT;
        for (p = 0; p < lv_nsub(PG_EDIT); p++) {
            lv.sub = (uint8_t)p;
            fits &= text_w(&AF_M, lv_page_name()) <= 82;
            for (k = 0; k < 4u; k++) {
                int32_t lo, hi;
                char value[16];
                const lv_knob_t *kb = lv_knob(k);
                int32_t v = lv_get(kb, &lo, &hi);
                lv_text(kb, value);
                fits &= lo <= v && v <= hi && text_w(&AF_S, lv_label(kb)) <= 60;
            }
        }
    }
    ok("edit UI: all engines expose valid descriptors and fitting labels", fits);
}
