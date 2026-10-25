/* SPDX-License-Identifier: GPL-3.0-only */
static void overlap_setup(void)
{
    power(); fx_quiet();
    trk[1].p[P_ATK] = trk[1].p[P_DIST] = trk[1].p[P_CHOR] = 0;
    trk[1].p[P_SUS] = trk[1].p[P_REL] = 127;
    trk[1].p[P_VOICE] = V_POLY;
    ncd_set_mode(1, 1); ncd_want[1] = 1;
}
static void drone_overlap_test(void)
{
    uint32_t b, i, energy = 0, baseline = 0, minimum = 0xFFFFFFFFu, voices, bounded = 1;
    overlap_setup();
    midi(0x93, 60, 100); perf_blocks(200);
    for (b = 0; b < 32u; b++) {
        blk();
        for (i = 0; i < CTL; i++) baseline += (uint32_t)abs(blk_out[2u * i + 1u]);
    }
    midi(0x83, 60, 0); blk(); midi(0x93, 67, 100); perf_blocks(3);
    ok("REPLACE overlap: old and new notes sound together; the whole part stays at unity",
       note_playing(&trk[1], 60) && note_playing(&trk[1], 67) && ncd_overlapping[1] &&
       ncd_fade_gain(1) == NCD_FADE_FULL && !ncd[1].pending);
    for (b = 0; b < NCD_FADE_BLOCKS + 64u; b++) {
        blk();
        bounded &= voices_busy() <= NVOICE;
        for (i = 0; i < CTL; i++) energy += (uint32_t)abs(blk_out[2u * i + 1u]);
        if ((b & 31u) == 31u) {
            if (energy < minimum) minimum = energy;
            energy = 0;
        }
    }
    printf("ncc: REPLACE overlap audio: baseline %u, lowest transition window %u\n", baseline, minimum);
    ok("REPLACE overlap: rendered audio has no silent handover, retires old note and stays within budget",
       bounded && baseline > 1000u && minimum > baseline / 8u &&
       !note_playing(&trk[1], 60) && note_playing(&trk[1], 67) && !ncd_overlapping[1]);

    overlap_setup(); midi(0x93, 60, 100); perf_blocks(100); midi(0x83, 60, 0); blk();
    midi(0x93, 60, 80); perf_blocks(3);
    for (i = voices = 0; i < NVOICE; i++) voices += trk[1].v[i].active && trk[1].v[i].note == 60u;
    ok("REPLACE overlap: the same pitch crossfades on separate voices instead of cutting its predecessor", voices == 2u);
    midi(0x83, 60, 0); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE overlap: old/new note-offs cannot release the replacement", note_playing(&trk[1], 60));

    overlap_setup();
    for (i = 0; i < 3u; i++) midi(0x93, 60u + i * 4u, 100);
    perf_blocks(100);
    for (i = 0; i < 3u; i++) midi(0x83, 60u + i * 4u, 0);
    blk();
    for (i = 0; i < 3u; i++) midi(0x93, 62u + i * 3u, 100);
    perf_blocks(3);
    ok("REPLACE overlap: triad-to-triad uses six voices without muting either chord", voices_busy() == 6u && !ncd[1].pending);
    perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE overlap: only the new triad remains after the transition", voices_busy() == 3u && ncd[1].n == 3u);
    for (i = 0; i < 3u; i++) midi(0x83, 62u + i * 3u, 0);
    blk(); midi(0x93, 70, 100); perf_blocks(3); midi(0xB3, 120, 0); perf_blocks(NCD_FADE_BLOCKS + 3u);
    ok("REPLACE overlap: panic cancels old tails, incoming ramps and pending chords",
       !gate_on(&trk[1]) && !ncd[1].pending && !ncd_overlapping[1]);

    overlap_setup();
    for (i = 0; i < NVOICE; i++) midi(0x93, 48u + i * 2u, 100);
    perf_blocks(100);
    for (i = 0; i < NVOICE; i++) midi(0x83, 48u + i * 2u, 0);
    blk(); midi(0x93, 72, 100); perf_blocks(20);
    ok("REPLACE overlap: a full voice budget uses the long smooth fallback and keeps old notes rendering",
       voices_busy() == NVOICE && ncd_fade[1].phase == NCD_FADE_OUT && ncd_fade_gain(1) > 30000u && !note_playing(&trk[1], 72));
    perf_blocks(2u * NCD_FADE_BLOCKS + 3u);
    ok("REPLACE overlap: capacity fallback eventually leaves only the new chord",
       voices_busy() == 1u && note_playing(&trk[1], 72) && !ncd[1].pending);

    for (i = 0; i < NELEM(LV_ENGS); i++) {
        uint32_t valid;
        overlap_setup(); set_engine_of(&trk[1], LV_ENGS[i]);
        trk[1].p[P_VOICE] = V_POLY;
        trk[1].p[P_ATK] = 0; trk[1].p[P_SUS] = 127;
        midi(0x93, 60, 100); perf_blocks(160); midi(0x83, 60, 0); blk();
        midi(0x93, 67, 100); perf_blocks(3);
        valid = trk_nvoice(&trk[1]) == 1u ? ncd_fade[1].phase == NCD_FADE_OUT :
                note_playing(&trk[1], 60) && note_playing(&trk[1], 67) && ncd_overlapping[1];
        perf_blocks(2u * NCD_FADE_BLOCKS + 3u);
        valid &= !note_playing(&trk[1], 60) && note_playing(&trk[1], 67) &&
                 !ncd[1].pending && !ncd_overlapping[1] && ncd_fade_gain(0) == NCD_FADE_FULL;
        ok("REPLACE overlap: every engine respects its own voice limit and completes the handover", valid);
    }
}

static void drone_fm6_phase_test(void)
{
    int32_t out[FM6_N], in[FM6_N];
    uint32_t i, phase, pass, valid = 1;
    for (pass = 0; pass < 2u; pass++) {
        phase = pass ? 0x80000001u : 0x7FFFFFFFu;
        for (i = 0; i < FM6_N; i++) in[i] = pass ? -1000000 : 1000000;
        fm6_op_run(out, in, (int32_t)phase, 100000, 32768, 32768, 0);
        for (i = 0; i < FM6_N; i++) {
            valid &= out[i] == fm6_sin((int32_t)(phase + (uint32_t)in[i]));
            phase += 100000u;
        }
    }
    ok("FM6 drones: modulated phases wrap at both signed boundaries without overflow or a sound change", valid);
}
