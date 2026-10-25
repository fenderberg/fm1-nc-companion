/* SPDX-License-Identifier: GPL-3.0-only */
/* Real queued MIDI, render endpoints and cancellation cases for REPLACE's longer transition. */
static void fade_setup(void)
{
    power(); fx_quiet();
    trk[1].p[P_ATK] = trk[1].p[P_DIST] = trk[1].p[P_CHOR] = 0;
    trk[1].p[P_SUS] = trk[1].p[P_REL] = 127;
    trk[1].p[P_VOICE] = V_UNISON;          /* no spare voices: exercise the capacity fallback */
    ncd_set_mode(1, 1); ncd_want[1] = 1;
    midi(0x93, 60, 100); perf_blocks(3);
}
static void drone_fade_test(void)
{
    uint32_t b, k, gain, last, smooth = 1, old_peak = 0, quiet_peak = 0, new_peak = 0;
    fade_setup();
    ok("REPLACE fade: first strike starts at unity without a musical fade delay",
       gate_on(&trk[1]) && ncd_fade_gain(1) == NCD_FADE_FULL && !ncd_fade[1].phase);
    for (b = 0; b < 160u; b++) {
        blk();
        if (b > 120u)
            for (k = 0; k < CTL; k++) {
                uint32_t a = (uint32_t)abs(blk_out[2u * k + 1u]);
                if (a > old_peak) old_peak = a;
            }
    }
    midi(0x83, 60, 0); blk(); midi(0x93, 67, 100);
    last = NCD_FADE_FULL;
    for (b = 0; b < NCD_FADE_BLOCKS; b++) {
        blk(); gain = ncd_fade_gain(1);
        smooth &= gain <= last && last - gain < 750u;
        smooth &= note_playing(&trk[1], 60) && !note_playing(&trk[1], 67);
        if (b == NCD_FADE_BLOCKS / 2u - 1u) smooth &= gain >= 16000u && gain <= 16700u;
        if (b >= NCD_FADE_BLOCKS - 3u)
            for (k = 0; k < CTL; k++) {
                uint32_t a = (uint32_t)abs(blk_out[2u * k + 1u]);
                if (a > quiet_peak) quiet_peak = a;
            }
        last = gain;
    }
    ok("REPLACE fade: old audio follows a monotonic ~250 ms curve to a rendered zero endpoint",
       smooth && !last && !trk[1].v[0].env_out && old_peak > 1000u && quiet_peak < old_peak / 8u);
    blk();
    ok("REPLACE fade: reuse slots only after silence; new note starts with zero amplitude",
       !note_playing(&trk[1], 60) && note_playing(&trk[1], 67) && !ncd_fade_gain(1));
    last = 0;
    for (b = 0; b < NCD_FADE_BLOCKS; b++) {
        blk(); gain = ncd_fade_gain(1);
        smooth &= gain >= last && gain - last < 750u;
        for (k = 0; k < CTL; k++) {
            uint32_t a = (uint32_t)abs(blk_out[2u * k + 1u]);
            if (a > new_peak) new_peak = a;
        }
        last = gain;
    }
    ok("REPLACE fade: new audio rises smoothly for ~250 ms, lands exactly at unity and remains sounding",
       smooth && gain == NCD_FADE_FULL && !ncd_fade[1].phase && new_peak > 1000u && note_playing(&trk[1], 67));

    fade_setup(); midi(0x83, 60, 0); blk();
    midi(0x93, 64, 100); midi(0x83, 64, 0); perf_blocks(20);
    midi(0x93, 65, 100); midi(0x83, 65, 0); perf_blocks(NCD_FADE_BLOCKS - 20u);
    ok("REPLACE fade: rapid strikes keep the original fade deadline", !ncd_fade_gain(1) && !ncd[1].wait);
    midi(0x93, 67, 100); blk();                     /* precisely at the zero boundary */
    ok("REPLACE fade: latest gesture wins even at the zero boundary; superseded notes never start",
       note_playing(&trk[1], 67) && !note_playing(&trk[1], 60) && !note_playing(&trk[1], 64) &&
       !note_playing(&trk[1], 65) && ncd[1].n == 1u && !ncd[1].pending && !ncd_fade_gain(1));

    fade_setup(); midi(0x83, 60, 0); midi(0x93, 67, 100); perf_blocks(12);
    midi(0xB3, 120, 0); blk(); perf_blocks(2u * NCD_FADE_BLOCKS);
    ok("REPLACE fade: panic during fade stops both the predecessor and queued replacement",
       !gate_on(&trk[1]) && !ncd[1].pending && !ncd[1].n && !ncd_fade[1].phase);
    fade_setup(); midi(0x83, 60, 0); midi(0x93, 67, 100); perf_blocks(12);
    ncd_want[1] = 0; midi(0x83, 67, 0); perf_blocks(2u * NCD_FADE_BLOCKS);
    ok("REPLACE fade: Drone off with no held keys cannot leave an old note or a muted part behind",
       !gate_on(&trk[1]) && !ncd[1].pending && !ncd_fade[1].phase);
    fade_setup(); midi(0x83, 60, 0); midi(0x93, 67, 100); perf_blocks(12);
    ncd_want[1] = 0; perf_blocks(2u * NCD_FADE_BLOCKS);
    ok("REPLACE fade: Drone off keeps the queued physical note and completes the fade",
       note_playing(&trk[1], 67) && !note_playing(&trk[1], 60) && !ncd_fade[1].phase);
    midi(0x83, 67, 0); blk();
    ok("REPLACE fade: after Drone off its note releases normally", !gate_on(&trk[1]));

    fade_setup(); midi(0x83, 60, 0); midi(0x93, 64, 100);
    perf_blocks(NCD_FADE_BLOCKS + 20u);
    gain = ncd_fade_gain(1);
    midi(0x83, 64, 0); midi(0x93, 67, 100); blk();
    ok("REPLACE fade: a new strike during fade-in reverses from the current gain without a jump",
       gain > 0u && gain < NCD_FADE_FULL && ncd_fade_gain(1) <= gain &&
       gain - ncd_fade_gain(1) < 100u && ncd[1].pending);
    perf_blocks(2u * NCD_FADE_BLOCKS + 3u);
    ok("REPLACE fade: after reversing, only the latest gesture reaches unity",
       note_playing(&trk[1], 67) && !note_playing(&trk[1], 64) && !ncd_fade[1].phase);

    for (k = 0; k < NELEM(LV_ENGS); k++) {
        uint32_t valid = 1;
        power(); fx_quiet(); set_engine_of(&trk[1], LV_ENGS[k]);
        trk[1].p[P_VOICE] = V_UNISON; trk[1].p[P_ATK] = 0; trk[1].p[P_SUS] = 127;
        ncd_set_mode(1, 1); ncd_want[1] = 1;
        midi(0x93, 60, 100); perf_blocks(160);
        midi(0x83, 60, 0); midi(0x93, 64, 100); midi(0x93, 67, 100); perf_blocks(10);
        valid &= ncd_fade[1].phase == NCD_FADE_OUT && ncd_fade_gain(0) == NCD_FADE_FULL;
        perf_blocks(2u * NCD_FADE_BLOCKS);
        valid &= !ncd[1].pending && note_playing(&trk[1], 67) &&
                 !note_playing(&trk[1], 60) && ncd_fade_gain(1) == NCD_FADE_FULL;
        ok("REPLACE fade: every engine shares the bounded transition; the other part stays at unity", valid);
    }
}
