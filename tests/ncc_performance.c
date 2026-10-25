/* SPDX-License-Identifier: GPL-3.0-only */
/* Included by ncc_test.c: real event queue, key input, UI and editor protocol; no duplicated router model. */
static void perf_blocks(uint32_t n) { while (n--) blk(); }
static void dual_button(void)
{
    lv_press(B_SCL);
    blk();
}
static void dual_test(void)
{
    int16_t cutoff;
    power();
    dual_button();
    ok("Dual UI: SCL enables Dual on the remembered source (CH3 initially)", ncc_dual_mode() == 1u && ncc_dual == 1u);
    midi(0x92, 60, 100); blk();
    ok("Dual CH3: one note plays both parts with separate owners", note_playing(&trk[0], 60) &&
       note_playing(&trk[1], 60) && midi_notes[16][60] == 1u && midi_notes[17][60] == 2u);
    lv_press(B_OCTUP);
    midi(0x82, 60, 0); blk();
    ok("Dual: focus change does not redirect either note-off", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    midi(0x93, 67, 100); blk();
    ok("Dual CH3: the other channel supplies no notes", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    cutoff = trk[1].p[P_E4];
    midi(0xB3, 71, 127); perf_blocks(30);
    ok("Dual: CCs on the inactive note channel still edit R", trk[1].p[P_E4] != cutoff);
    midi(0xE2, 0, 96); blk();
    ok("Dual: CH3 bend affects L alone, including on the next note", midi_bend_target[0] && !midi_bend_target[1]);
    midi(0x92, 65, 100); blk();
    ok("Dual: note fanout does not copy or clear bend", midi_bend_target[0] && !midi_bend_target[1]);
    lv_press(B_GLO); lv_press(B_PLAY); t_turn(EN_K1, 1); blk();
    ok("Dual UI: GLO selects CH4 and fades the old notes", ncc_dual == 2u && !gate_on(&trk[0]) && !gate_on(&trk[1]));
    lv_press(B_HOME);
    midi(0x92, 69, 100); blk();
    ok("Dual CH4: CH3 is no longer a note source", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    midi(0x93, 72, 100); blk();
    midi(0x82, 65, 0); blk();
    ok("Dual CH4: both play; an old CH3 off cannot stop them", note_playing(&trk[0], 72) && note_playing(&trk[1], 72));
    midi(0x83, 72, 0); blk();
    ok("Dual: CH4 off releases both parts", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    dual_button();
    midi(0x92, 60, 100); blk();
    ok("Dual OFF: normal separate routing restored", !ncc_dual && note_playing(&trk[0], 60) && !gate_on(&trk[1]));
    dual_button();
    ok("Dual UI: SCL restores CH4 after turning Dual off", ncc_dual == 2u && ncc_dual_source() == 2u);

    power(); ncc_set_dual(1); blk();
    midi(0xB3, 64, 127); midi(0x92, 60, 100); blk();
    midi(0x82, 60, 0); blk();
    ok("Dual: only R's own sustain pedal holds R", !gate_on(&trk[0]) && gate_on(&trk[1]));
    midi(0xB3, 64, 0); blk();
    ok("Dual: R pedal-up releases its private owner", !gate_on(&trk[1]) && !midi_owners[1]);
    midi(0x92, 64, 100); blk(); midi(0xB2, 120, 0); blk();
    ok("Dual: L All Sound Off leaves R alone", !gate_on(&trk[0]) && note_playing(&trk[1], 64));
    midi(0x82, 64, 0); blk();
    ok("Dual: note-off after one part's panic releases the other owner", !gate_on(&trk[1]));
    midi(0x92, 67, 100); blk(); midi(0xB3, 123, 0); blk();
    ok("Dual: R All Notes Off does not release L", note_playing(&trk[0], 67) && !gate_on(&trk[1]));
    midi(0x82, 67, 0); blk();

    key_down(12); blk();
    {
        uint32_t note = kb_note[12];
        ok("Dual keys: one local key starts both layers", note_playing(&trk[0], note) && note_playing(&trk[1], note));
        midi(0x92, note, 100); blk();
        lv_press(B_OCTUP); key_up(12); blk();
        ok("Dual keys: MIDI keeps both notes after local release and focus change", gate_on(&trk[0]) && gate_on(&trk[1]));
        midi(0x82, note, 0); blk();
        ok("Dual keys: last MIDI owner releases both", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    }
    key_down(10); blk(); ncc_set_dual(2); blk(); key_up(10); blk();
    ok("Dual: source change with a key held leaves no stuck owner", !gate_on(&trk[0]) && !gate_on(&trk[1]) &&
       !midi_owners[0] && !midi_owners[1] && !ncc_key_mask);
    trk[0].p[P_VOICE] = trk[1].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHRD] = CH_MAJ; trk[1].p[P_CHRD] = CH_MIN;
    midi(0x93, 60, 100); blk();
    ok("Dual: each part builds its own chord and releases the original snapshot", note_playing(&trk[0], 64) &&
       note_playing(&trk[1], 63));
    trk[0].p[P_CHRD] = trk[1].p[P_CHRD] = CH_OFF;
    midi(0x83, 60, 0); blk();
    ok("Dual: changing chord settings does not strand the old chord", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    midi_enqueue(0x09u | 0x93u << 8 | 60u << 16 | 100u << 24, 2u); blk();
    ok("Dual: TRS uses the same chosen source", gate_on(&trk[0]) && gate_on(&trk[1]));
    midi_in_overflow = 1; blk();
    ok("Dual: queue overflow clears both layers and their owners", !gate_on(&trk[0]) && !gate_on(&trk[1]) &&
       !midi_owners[0] && !midi_owners[1]);
    midi(0x93, 62, 100); blk(); seq_stop(); blk();
    ok("Dual: Stop clears both layers", !gate_on(&trk[0]) && !gate_on(&trk[1]));
}

static void replace_drone_test(void)
{
    uint32_t mode;
    for (mode = V_POLY; mode <= V_UNISON; mode++) {
        track_t *t;
        power(); t = &trk[1];
        t->p[P_VOICE] = (int16_t)mode; t->p[P_REL] = 127;
        ncd_set_mode(1, 1); ncd_want[1] = 1;
        midi(0x93, 60, 100); perf_blocks(NCD_FADE_BLOCKS + 3u); midi(0x83, 60, 0); blk();
        midi(0x93, 67, 100); blk();
        ok("REPLACE: render the old voice's fade before starting its replacement", ncd[1].pending && note_playing(t, 60) && !note_playing(t, 67));
        perf_blocks(NCD_FADE_BLOCKS + 2u);
        ok("REPLACE: every voice mode keeps only the new drone, with long RELEASE", !note_playing(t, 60) &&
           note_playing(t, 67) && ncd[1].n == 1u && ncd[1].note[0] == 67u);
        midi(0x83, 60, 0); midi(0x83, 67, 0); perf_blocks(NCD_FADE_BLOCKS + 3u);
        ok("REPLACE: old and new note-offs leave the new drone held", note_playing(t, 67));
        midi(0x93, 67, 80); perf_blocks(NCD_FADE_BLOCKS + 3u);
        ok("REPLACE: the same note retriggers instead of toggling off", note_playing(t, 67) && ncd[1].n == 1u);
    }
    power();
    lv_press(B_ARP); lv_press(B_PLAY); t_turn(EN_K4, 1);
    ok("REPLACE UI: ARP page 2's fourth knob chooses REPLACE", lv.page == PG_ARP && lv.sub == 1u && ncd_mode(0));
    ncd_want[0] = 1; trk[0].p[P_AHOLD] = 1;
    midi(0x92, 60, 100); perf_blocks(NCD_FADE_BLOCKS + 3u); midi(0x82, 60, 0); blk(); midi(0x92, 67, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    midi(0x82, 60, 0); blk();
    ok("REPLACE arp: HOLD cannot retain the old note or consume the new key count", trk[0].nheld == 1u &&
       trk[0].held[0] == 67u && trk[0].arp_phys == 1u);
    power(); ncc_set_dual(1); ncd_set_mode(1, 1); ncd_want[1] = 1;
    ncd_want[0] = 1;                                  /* STACK on L keeps the arp's earlier chord */
    trk[0].p[P_AMODE] = AM_UP; trk[0].p[P_AHOLD] = 1;
    midi(0x92, 60, 100); perf_blocks(NCD_FADE_BLOCKS + 3u); midi(0x82, 60, 0); blk(); midi(0x92, 67, 100); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("Dual: L's STACK arp keeps its chord while R replaces its drone", trk[0].nheld == 2u &&
       ncd[1].n == 1u && note_playing(&trk[1], 67) && !note_playing(&trk[1], 60));
    midi(0xB3, 120, 0); blk();
    ok("REPLACE: panic clears pending replacement and drone bookkeeping", !ncd[1].pending && !ncd[1].n && !gate_on(&trk[1]));
    midi(0x92, 72, 100); midi(0xB3, 123, 0); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE: All Notes Off cancels a pending replacement", !ncd[1].pending && !ncd[1].n && !gate_on(&trk[1]));
    power(); ncd_set_mode(0, 1); ncd_want[0] = 1;
    midi(0x92, 60, 100); blk();
    ncd_want[0] = 0; perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE: Drone off during the fade still plays a physically held new note", note_playing(&trk[0], 60));
    midi(0x82, 60, 0); blk();
    ok("REPLACE: after Drone off that held note has an ordinary note-off", !gate_on(&trk[0]));
    for (mode = 0; mode < 2u; mode++) {
        uint32_t note;
        power(); ncc_set_dual(1); ncd_set_mode(1, mode); ncd_want[1] = 1; blk();
        key_down(12); perf_blocks(NCD_FADE_BLOCKS + 3u); note = kb_note[12];
        ncd_want[1] = 0; blk();
        ok("Drone off: a physically held Dual key stays sounding in either mode", note_playing(&trk[1], note));
        key_up(12); blk();
        ok("Drone off: releasing that Dual key ends both layers", !gate_on(&trk[0]) && !gate_on(&trk[1]));
    }
}

static void default_sends_test(void)
{
    uint32_t part, i, all = 1, other = 1;
    power();
    ok("sound defaults: boot sounds have no delay or reverb", !trk[0].p[P_DLY] && !trk[0].p[P_REV] &&
       !trk[1].p[P_DLY] && !trk[1].p[P_REV]);
    for (part = 0; part < 2u; part++) {
        lv_focus(part); nl_build();
        for (i = 0; i < nl.n; i++) {
            trk[part ^ 1u].p[P_DLY] = 59; trk[part ^ 1u].p[P_REV] = 73;
            nl_load(i);
            all &= !trk[part].p[P_DLY] && !trk[part].p[P_REV];
            other &= trk[part ^ 1u].p[P_DLY] == 59 && trk[part ^ 1u].p[P_REV] == 73;
        }
    }
    ok("sound defaults: every library sound starts without delay/reverb on either part", all);
    ok("sound defaults: loading a sound preserves the other part's sends", other);
    lv_focus(0); trk[0].p[P_DLY] = 80; trk[0].p[P_REV] = 90;
    perf_blocks(5);
    ok("sound defaults: effects can be turned up and stay up", trk[0].p[P_DLY] == 80 && trk[0].p[P_REV] == 90);
    {
        int rc = up_store(3, "WET TEST");
        int16_t values[P_COUNT];
        up_values(up_rec(3), values);
        ok("sound defaults: a wet user sound is stored intact (host RAM store)", (rc == 0 || rc == 3) &&
           up_used(3) && values[P_DLY] == 80 && values[P_REV] == 90);
    }
    ok("sound defaults: the stored user sound loads", !up_load(3));
    ok("sound defaults: loading a stored wet user sound starts without sends", !trk[0].p[P_DLY] && !trk[0].p[P_REV]);
    trk[0].p[P_DLY] = 40; trk[0].p[P_REV] = 41;
    apply_preset_to(&trk[0], 0); undo_swap();
    ok("sound defaults: undo restores deliberately adjusted sends", trk[0].p[P_DLY] == 40 && trk[0].p[P_REV] == 41);
    trk[0].p[P_DLY] = 20; trk[0].p[P_REV] = 21;
    midi_pc_load(&trk[0], 0);
    ok("sound defaults: Program Change resets sends", !trk[0].p[P_DLY] && !trk[0].p[P_REV]);
    fm6_bank_read = stub_bank32; fm6_bank_used = 0xFFFFFFFFu;
    set_engine_of(&trk[0], ENGI_FM6); trk[0].p[P_DLY] = 90; trk[0].p[P_REV] = 91;
    midi_pc_load(&trk[0], preset_shown(ENGI_FM6));
    ok("sound defaults: bank Program Change resets sends", !trk[0].p[P_DLY] && !trk[0].p[P_REV]);
    fm6_bank_read = 0; fm6_bank_used = 0;
}

static void performance_protocol_test(void)
{
    uint8_t q[4];
    static project_t snapshot;
    static project_store_t stored;
    power();
    q[0] = 4; q[1] = 2;
    ncc_req(ED_NCC_SET, q, 2);
    ok("performance protocol: SET Dual CH4", !ed_out[1] && ncc_dual_mode() == 2u);
    q[0] = 5; q[1] = 1; q[2] = 1; q[3] = 1;
    ncc_req(ED_NCC_SET, q, 4);
    ok("performance protocol: SET R Drone REPLACE on", !ed_out[1] && ncd_mode(1) && ncd_want[1]);
    ncc_req(ED_NCC_PARTS, 0, 0);
    ok("performance protocol: append-only versioned PARTS trailer", ed_out[ed_n - 6u] == 1u &&
       ed_out[ed_n - 5u] == 2u && ed_out[ed_n - 2u] == 1u && ed_out[ed_n - 1u] == 1u);
    q[0] = 4; q[1] = 3;
    ncc_req(ED_NCC_SET, q, 2);
    ok("performance protocol: unknown Dual modes rejected", ed_out[1] == 1u && ncc_dual_mode() == 2u);
    q[0] = 5; q[1] = 2; q[2] = 0; q[3] = 0;
    ncc_req(ED_NCC_SET, q, 4);
    ok("performance protocol: unknown parts rejected", ed_out[1] == 1u);
    trk[0].p[P_DLY] = 60; trk[0].p[P_REV] = 61;
    project_capture(&snapshot);
    ok("performance storage: existing project format packs", proj_pack(&stored, &snapshot));
    ok("performance storage: existing project bytes unpack the performance choices", proj_unpack(&snapshot,
       stored.raw, sizeof stored) && snapshot.g[G_SYNC] == song.g[G_SYNC]);
    ncc_set_dual(0); ncd_set_mode(1, 0); trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    ok("performance storage: project restores modes and chosen effect sends", !project_restore_runtime(&snapshot) &&
       ncc_dual_mode() == 2u && ncd_mode(1) && trk[0].p[P_DLY] == 60 && trk[0].p[P_REV] == 61);
    ncc_set_dual(0); project_capture(&snapshot);
    ok("performance storage: Dual off keeps its source through project bytes", proj_pack(&stored, &snapshot) &&
       proj_unpack(&snapshot, stored.raw, sizeof stored) && !project_restore_runtime(&snapshot) &&
       !ncc_dual_mode() && ncc_dual_source() == 2u);
    song.g[G_SYNC] = NCC_PERF_MARK | 2u;            /* earlier build did not store the remembered-source bit */
    ncc_set_dual(0);
    ok("performance storage: older marked CH4 settings remain usable with SCL", ncc_dual_source() == 2u);
    song.g[G_SYNC] = 0;
    ok("performance storage: unmarked old projects use OFF / STACK", !ncc_dual_mode() && !ncd_mode(0) && !ncd_mode(1));
    song.g[G_SYNC] = 127;
    ok("performance storage: corrupt choices use safe defaults", !ncc_dual_mode() && !ncd_mode(0) && !ncd_mode(1));
    ok("performance screen: all ARP/HOLD/Drone badges fit together", text_w(&AF_S, "ARP") + text_w(&AF_S, "H") +
       text_w(&AF_S, "DR1") + 38 <= 98 && text_w(&AF_S, "REPLACE") <= 58);
}

#include "ncc_panel_chords.c"
#include "ncc_drone_fade_test.c"
#include "ncc_drone_overlap_test.c"

static void performance_test(void)
{
    page_button_test();
    dual_test();
    replace_drone_test();
    replace_chord_test();
    drone_fade_test();
    drone_overlap_test();
    drone_fm6_phase_test();
    settings_edit_test();
    default_sends_test();
    performance_protocol_test();
}
