/* SPDX-License-Identifier: GPL-3.0-only */
/* Extended NCC panel and chord gestures: physical releases, not time windows or sustain, delimit REPLACE chords. */
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
    t_turn(EN_SELECT, 1); t_turn(EN_K1, 2); t_turn(EN_K2, 5); t_turn(EN_K3, 2); t_turn(EN_K4, 1);
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
    t_turn(EN_SELECT, 1); before = trk[1].p[P_E4]; other = trk[0].p[P_E4];
    trk[1].p[P_DLY] = 53; trk[1].p[P_REV] = 54;
    t_turn(EN_K1, -5);
    ok("edit UI: cutoff edits only focused R and keeps adjusted sends", trk[1].p[P_E4] < before &&
       trk[0].p[P_E4] == other && trk[1].p[P_DLY] == 53 && trk[1].p[P_REV] == 54);
    undo_swap();
    ok("edit UI: parameter edits support the existing undo snapshot", trk[1].p[P_E4] == before);
    lv_press(B_HOME); lv_press(B_EDIT); t_turn(EN_SELECT, 1); t_turn(EN_SELECT, 1);
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
