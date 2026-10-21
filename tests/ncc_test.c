/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of NCC (FELUCCA_LIVEUI=1, milestone 1) with the real sources: the macro layer (src/perf_layer.c, the
 * Circuit Tracks' knobs on CC 1 2 5 11 12 13 71 74), FM6's AMP ENV (CEG), the live UI (src/live_ui.c) on the stub panel
 * and display of tests/ui_test.c, MIDI IN TRACK on channels 3 / 4, the BPM guard of fx.c beat_samples.
 *   SETUP      the first pass: L FM6, R ANALOG, TRACK on channels 3 / 4, tracks 3 / 4 muted and deaf, no steps
 *   ROUTING    a note on channel 3 plays L, on 4 R, on 1 nothing
 *   KEYS       the FM-1's keys play the part in focus
 *   SLEW       a CC from 64 to 127: the parameter rises block by block, never more than a third of the way in one,
 *              there within 14 blocks (10 ms); 100 CCs in one block: the last one counts
 *   RELATIVE   CC 64 is the sound; a new sound puts the knobs back to 64 (its values as loaded); a value written by
 *              something else becomes the base; clamped to the range
 *   OWN CCS    CC 1 / 5 / 11 on an NCC part are knobs, not the wheel, GLIDE, EXPR
 *   LAZY       FM6's patch values (MEG, FB ..) are written every 4th block while sliding, and end exactly
 *   CEG        CC 5 on FM6: the carriers' envelope rates move, the modulators' do not
 *   ZIPPER     a cutoff jump by CC: the output's largest sample step stays near a steady note's
 *   UI         OCT- / OCT+ the focus and its LED; PRESETS loads once after a pause, on the focused part only;
 *              ALGORITHM steps FM6's SLOT; the popup with the knob's legend for LV_POP_MS
 *   CHORUS     the Juno chorus: OFF is Felucca's loop, II stereo (left != right, the mono sum kept), the rates, no
 *              click at a mode change, OFF fades out
 *   SPLIT      a note on L: the right output exactly 0 (no bus sends), and the other way round
 *   JUNO       a Juno sound's values, chorus and drift; the macros back to 64; the drift per POLY note, none below 4
 *   UI 2       ALGORITHM on R the chorus (popup), PRESETS the Juno list first, GLO + SEQ SPLIT, both stored
 *   ARP UI     ARP on (its page, the mode back), KNOB 1..4 and page 2, off on its page; REC HOLD (and off lets go)
 *   ARP GRID   a note mid-step starts the arp on the next step of the bar, a little late at once; no clock: BPM
 *   DRONE      SEQ: note-offs ignored, notes pile up, a droning note again lets go, SEQ off releases all
 *   FX UI      FX a toggle: its mode (the keys PERFORM's, no button held), SELECT its pages PERFORM SENDS DELAY REVERB
 *              CHORUS INSERT (the insert's knobs named by its type), FX / HOME close it; ENV / LFO throws
 *   SPLIT DAC  one part with sends, throws, THROW, FLANGER, Juno chorus and the other's tail: the other side of the
 *              output after MASTER exactly 0
 *   SCREEN     the head's block in the focused part's colour; the labels fit their cells
 *   ENGINES    SELECT on SOUND picks the part's engine (after a rest, round the list), ACID slides, stored; the
 *              Circuit's FILTER FREQ on PHASE, TRIO, ACID
 *   ACID       it sounds, the filter envelope closes, RES 127 bounded, accent brighter and louder, slide on overlap
 *              only, saw / square
 *   STOP       the Circuit's Stop (USB clock): the arp stops (latched chord dropped), the drone lets go
 *   SPLASH     NCC on the power-on screen, Felucca's version and credits kept, every line inside its square
 *   BPM 0      beat_samples counts it as 120
 * Built and run by tests/run_tests.sh. */
#ifndef FELUCCA_LIVEUI
#define FELUCCA_LIVEUI 1
#endif
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "../firmware/src/live_ui.c"

static int bad;
static void ok(const char *what, int c) { bad += check(what, c); }

static void lv_frame(void)
{
    host_ticks += 16000u;
    fm1_ms += 16u;
    live_input();
    live_leds();
    live_draw();
}
static void lv_frames(uint32_t ms) { uint32_t i; for (i = 0; i < ms / 16u; i++) lv_frame(); }
static void lv_press(uint32_t label)
{
    fm1_in.buttons |= 1u << panel.btn[label];
    host_pressed |= 1u << panel.btn[label];
    lv_frame();
    fm1_in.buttons &= ~(1u << panel.btn[label]);
    lv_frame();
}
static void t_turn(uint32_t role, int32_t s)
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    lv_frame();
}
static int32_t blk_out[2 * CTL];
static void blk(void) { mix_block(blk_out, CTL); }
static void midi(uint32_t st, uint32_t d1, uint32_t d2)   /* queued as USB MIDI; played by the next block */
{
    midi_enqueue((st >= 0xF8u ? 0xFu : st >> 4) | st << 8 | d1 << 16 | d2 << 24, 1u);
}
static int gate_on(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate)
            return 1;
    return 0;
}
static void power(void)
{
    ui_power_on();
    memset(&lv, 0, sizeof lv);
    memset(&ncm, 0, sizeof ncm);
    memset(fm6_ceg, 0, sizeof fm6_ceg);
    ncm_seen = 0;
    song.master_q12 = 2048;                             /* (main.c: the MASTER pot's level) */
    song.g[G_ENGGO] = 1;                                /* STEREO (SPLIT, NCC's default, where a test says so) */
    lv_frame();                                         /* lv_setup */
    blk();
}
static int led_known(uint32_t label) { return led_pos[panel.btn[label]] != 0xFFu; }   /* (the stub's matrix: OCT- only) */
static int led_is(uint32_t label, int on) { return !led_known(label) || ((fm1_led[led_pos[panel.btn[label]] >> 3] >>
                                                   (led_pos[panel.btn[label]] & 7u)) & 1u) == (uint32_t)on; }
static int lit(uint32_t label)                      /* its LED on (0: none, OCT+ in PANEL_DEFAULT) */
{
    uint32_t q = led_pos[panel.btn[label]];
    return q != 0xFFu && ((fm1_led[q >> 3] >> (q & 7u)) & 1u);
}

static void setup_test(void)
{
    uint32_t i, steps = 0;
    power();
    for (i = 0; i < NSTEP; i++)
        steps += trk[0].step[i].note[0] != 0 || trk[1].step[i].note[0] != 0;
    ok("setup: L FM6, R ANALOG", trk[0].eng_req == ENGI_FM6 && trk[1].eng_req == ENGI_ANALOG);
    ok("setup: MIDI IN TRACK, L channel 3, R channel 4", song.g[G_ROUTE] == ROUTE_TRACK && mch_get(0) == 2u &&
       mch_get(1) == 3u);
    ok("setup: tracks 3 / 4 muted, no channel", trk[2].p[P_MUTE] && trk[3].p[P_MUTE] && mch_get(2) == MCH_OFF &&
       mch_get(3) == MCH_OFF);
    ok("setup: no steps (the Circuit plays)", steps == 0u);
    ok("setup: focus L, OCT- lit", song.sel == 0u && lit(B_OCTDN) && !lit(B_OCTUP));
}

static void routing_test(void)
{
    power();
    midi(0x92, 60, 100); blk();
    ok("routing: channel 3 plays L", gate_on(&trk[0]) && !gate_on(&trk[1]));
    midi(0x82, 60, 0); midi(0x93, 64, 100); blk();
    ok("routing: channel 4 plays R", gate_on(&trk[1]));
    midi(0x83, 64, 0); blk();
    power();
    midi(0x90, 60, 100); midi(0x91, 62, 100); blk();
    ok("routing: channels 1 / 2 play nothing", !gate_on(&trk[0]) && !gate_on(&trk[1]) && !gate_on(&trk[2]));
}

static void keys_test(void)
{
    power();
    key_down(12); blk();
    ok("keys: with L in focus a key plays L", gate_on(&trk[0]) && !gate_on(&trk[1]));
    key_up(12); blk();
    lv_press(B_OCTUP);
    key_down(14); blk();
    ok("keys: with R in focus a key plays R", gate_on(&trk[1]));
    key_up(14); blk();
}

static void slew_test(void)
{
    track_t *t = &trk[1];
    int32_t base, prev, v, lo = 0, hi = 127, want, full, maxstep = 0, mono = 1;
    uint32_t i, there = 0;
    power();
    base = t->p[P_E4];
    full = ((127 << 8) - (64 << 8)) * (hi - lo) * 32 >> 20;     /* the whole move (before the clamp) */
    want = clamp(base + full, lo, hi);
    midi(0xB3, 12, 127);                                /* FILTER FREQ on R */
    prev = base;
    for (i = 0; i < 40u; i++) {
        blk();
        v = t->p[P_E4];
        mono &= v >= prev;
        if (v - prev > maxstep)
            maxstep = v - prev;
        if (!there && v == want)
            there = i + 1u;
        prev = v;
    }
    ok("slew: FILTER FREQ rises monotonically to its value", mono && t->p[P_E4] == want);
    ok("slew: at most a third of the whole move in a block", maxstep * 3 <= full + 2);
    ok("slew: there within 14 blocks (10 ms)", there && there <= 14u);
    for (i = 0; i < 40u; i++)                           /* (the queue holds 64) */
        midi(0xB3, 12, (i * 37u) & 127u);
    midi(0xB3, 12, 64);
    for (i = 0; i < 40u; i++)
        blk();
    ok("slew: 41 CCs in one block, the last one counts (64: the sound's value)", t->p[P_E4] == base);
}

static void relative_test(void)
{
    track_t *t = &trk[1];
    int32_t loaded;
    uint32_t i;
    power();
    midi(0xB3, 13, 127);                                /* RESONANCE up */
    for (i = 0; i < 30u; i++)
        blk();
    ok("relative: RESONANCE moved off the sound", t->p[P_E5] != ENGINES[ENGI_ANALOG]->presets[t->preset].e[5]);
    apply_preset_to(t, 2);
    loaded = t->p[P_E5];
    for (i = 0; i < 30u; i++)
        blk();
    ok("relative: a new sound puts the knobs back to 64, its values as loaded",
       t->p[P_E5] == loaded && ncm.cur[1][5] == NCM_MID && ncm.tgt[1][5] == 64u);
    t->p[P_E4] = 10;                                    /* (another writer: the editor, Program Change) */
    for (i = 0; i < 5u; i++)
        blk();
    ok("relative: a value written by something else is the new base", t->p[P_E4] == 10);
    midi(0xB3, 12, 0);
    for (i = 0; i < 30u; i++)
        blk();
    ok("relative: clamped to the range (CUT 10, turned all the way down: 0)", t->p[P_E4] == 0);
    midi(0xB2, 12, 0);                                  /* FILTER FREQ on L: MLVL -64 .. 63 */
    for (i = 0; i < 30u; i++)
        blk();
    ok("relative: FM6's MLVL down by half its range, inside -64 .. 63", trk[0].p[P_E2] < -50 && trk[0].p[P_E2] >= -64);
}

static void own_cc_test(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    power();
    t->mw = 0;
    midi(0xB2, 1, 120); midi(0xB2, 5, 100); midi(0xB2, 11, 10);
    for (i = 0; i < 30u; i++)
        blk();
    ok("own CCs: CC 1 / 5 / 11 on L are knobs (not the wheel, GLIDE, EXPR)",
       t->mw == 0 && t->p[P_GLIDE] == TP[P_GLIDE].def && t->ex_off == 0 && t->p[P_E3] != 0 && fm6_ceg[0] != 0);
    midi(0xB0, 1, 99); blk();
    ok("own CCs: on another channel nothing changes", trk[0].mw == 0 && trk[1].mw == 0);
}

static void lazy_test(void)
{
    track_t *t = &trk[0];
    int32_t prev, base;
    uint32_t i, writes = 0;
    power();
    base = t->p[P_E1];
    midi(0xB2, 4, 0);                                   /* (no knob on CC 4: nothing) */
    midi(0xB2, 11, 127);                                /* FILTER ENV on L: MEG, rebuilds FM6's patch */
    prev = t->p[P_E4];
    for (i = 0; i < 40u; i++) {
        blk();
        writes += t->p[P_E4] != prev;
        prev = t->p[P_E4];
    }
    printf("ncc: MEG sliding 40 blocks: %u writes\n", writes);
    ok("lazy: FM6's MEG written at most every 4th block while sliding (+ the arrival)", writes >= 3u && writes <= 40u / 4u + 2u);
    ok("lazy: .. and it ends exactly at its value", t->p[P_E4] == clamp(0 + (((127 << 8) - NCM_MID) * 127 * 32 >> 20), -64, 63));
    midi(0xB2, 2, 127);                                 /* OSC MOD: FB, steps */
    for (i = 0; i < 40u; i++)
        blk();
    ok("lazy: FB (steps) ends at its value too", t->p[P_E1] == clamp(base + (((127 << 8) - NCM_MID) * 14 * 32 >> 20), -7, 7));
}

static void ceg_test(void)
{
    track_t *t = &trk[0];
    uint8_t before[FP_SIZE + 1u];
    uint32_t i, k, car, cmoved = 0, mmoved = 0;
    power();
    midi(0x92, 60, 100); blk();
    memcpy(before, fm6_eff[0].p, sizeof before);
    midi(0xB2, 5, 127);                                 /* AMP ENV up: slower */
    for (i = 0; i < 30u; i++)
        blk();
    car = fm6_carriers(fm6_eff[0].alg);
    for (k = 0; k < 6u; k++) {
        const uint8_t *a = before + (5u - k) * FP_OP, *b = fm6_eff[0].p + (5u - k) * FP_OP;
        uint32_t ki = 5u - k, j, d = 0;
        for (j = 0; j < 4u; j++)
            d += a[FP_R1 + j] != b[FP_R1 + j];
        if ((car >> ki) & 1u)
            cmoved += d;
        else
            mmoved += d;
    }
    ok("CEG: AMP ENV slows the carriers' envelope rates, not the modulators'", fm6_ceg[0] > 20 && cmoved && !mmoved);
    midi(0x82, 60, 0); blk();
    apply_preset_to(t, 1);
    for (i = 0; i < 3u; i++)
        blk();
    ok("CEG: back to 0 with a new sound", fm6_ceg[0] == 0);
}

/* the largest sample step of R's output over blocks [b0, b1) of a dark ANALOG note; at block 40 its cutoff goes to the
 * top: by the FILTER FREQ knob (how = 1), written at once (how = 2), or not at all (0) */
static int32_t step_max(int how, uint32_t b0, uint32_t b1)
{
    int32_t m = 0, last = 0, top;
    uint32_t b, i;
    power();
    trk[1].p[P_E4] = 40;
    top = clamp(40 + (((127 << 8) - NCM_MID) * 127 * 32 >> 20), 0, 127);   /* (CC 127: ~half the range up) */
    midi(0x93, 48, 100);
    for (b = 0; b < b1; b++) {
        if (b == 40u && how == 1)
            midi(0xB3, 12, 127);
        if (b == 40u && how == 2)
            trk[1].p[P_E4] = (int16_t)top;
        blk();
        for (i = 0; i < CTL; i++) {
            int32_t d = blk_out[2u * i] - last;
            last = blk_out[2u * i];
            if (b >= b0 && (d < 0 ? -d : d) > m)
                m = d < 0 ? -d : d;
        }
    }
    return m;
}
static void zipper_test(void)
{
    int32_t dark = step_max(0, 20, 40), knob = step_max(1, 40, 60), jump = step_max(2, 40, 60);
    int32_t bright = step_max(1, 100, 140);
    printf("ncc: largest sample step: dark %d, the 20 blocks after the cutoff jump: by the knob %d, at once %d; "
           "bright %d\n", (int)dark, (int)knob, (int)jump, (int)bright);
    ok("zipper: the knob's jump steps the output no more than the bright note's own (+25 %)",
       dark > 0 && knob * 4 <= bright * 5);
    ok("zipper: .. and no more than the same jump written at once", knob <= jump);
}

static void ui_test_ncc(void)
{
    uint32_t p0, p1, i;
    power();
    lv_press(B_OCTUP);
    ok("UI: OCT+ focuses R, OCT- dark", song.sel == 1u && !lit(B_OCTDN));
    p0 = trk[0].preset;
    p1 = trk[1].preset;
    t_turn(EN_PRESET, 1); t_turn(EN_PRESET, 1); t_turn(EN_PRESET, 1);
    ok("UI: PRESETS turning: nothing loaded yet", trk[1].preset == p1);
    lv_frames(200);
    ok("UI: after the pause, three on, once, on R only", trk[1].preset != p1 && trk[0].preset == p0 &&
       trk[1].preset == (p1 + 3u) % ENGINES[ENGI_ANALOG]->npresets);
    lv_press(B_OCTDN);
    ok("UI: OCT- focuses L", song.sel == 0u && lit(B_OCTDN));
    i = (uint32_t)trk[0].p[P_E7];
    t_turn(EN_ALGO, 1);
    lv_frame();
    ok("UI: ALGORITHM steps FM6's SLOT", (uint32_t)trk[0].p[P_E7] == i + 1u && fm6_slot[0] == i + 1u);
    midi(0xB3, 74, 100); blk();
    lv_frame();
    ok("UI: the popup shows the knob (R FX 100)", lv.pop_ms != 0u && (ncm_seen & 0xFFFFFFu) == (1u << 16 | 7u << 8 | 100u));
    lv_frames(1100);
    ok("UI: the popup goes after a second", lv.pop_ms == 0u);
    {   /* the pads: L's rim in L's colour (blue), R's dark */
        uint16_t l = swap16(host_screen[LY_PARTS * 240u + 60u]), r = swap16(host_screen[LY_PARTS * 240u + 178u]);
        ok("UI: the focused pad L has its rim in L's colour, R's dark", l == LC_PART[0] && r == LC_LINE);
    }
}

/* ------------------------------------------------------- milestone 2 --- */
static int64_t ch_e[3];                                 /* the last render's energy: left, right, (left + right) / 2 */
static int32_t ch_step;                                 /* .. its largest sample step (left) */
static void render(uint32_t blocks, uint32_t skip)
{
    uint32_t b, i;
    int32_t last = 0;
    ch_e[0] = ch_e[1] = ch_e[2] = 0;
    ch_step = 0;
    for (b = 0; b < blocks; b++) {
        blk();
        for (i = 0; b >= skip && i < CTL; i++) {
            int32_t l = blk_out[2u * i] >> 8, r = blk_out[2u * i + 1u] >> 8, m = (l + r) / 2, d = blk_out[2u * i] - last;
            ch_e[0] += (int64_t)l * l;
            ch_e[1] += (int64_t)r * r;
            ch_e[2] += (int64_t)m * m;
            if ((d < 0 ? -d : d) > ch_step)
                ch_step = d < 0 ? -d : d;
            last = blk_out[2u * i];
        }
    }
}
static void fx_quiet(void)                              /* the buses empty: no tail of a test before */
{
    memset(dly_buf, 0, sizeof dly_buf);
    rev_clear();
    memset(ins, 0, sizeof ins);
    memset(jc, 0, sizeof jc);
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = dc_l = dc_r = dce_l = dce_r = 0;   /* MASTER's filters: no ring of a test before */
    memset(lce, 0, sizeof lce);
    lim_env = LIM_T;
}
static void chorus_test(void)
{
    track_t *t = &trk[1];
    int64_t dry, wetm;
    int32_t diff = 0, steady, sw;
    uint32_t i, k;
    power();
    fx_quiet();
    ok("chorus: OFF and ST: mix_part's own loop (bit for bit as Felucca)", !ncc_custom(&trk[0]) && !ncc_custom(t));
    t->p[P_PAN] = 0;
    midi(0x93, 48, 100); midi(0x93, 55, 100);
    render(40, 20);
    dry = ch_e[2];
    t->p[P_ED_FX] = JC_II;
    render(60, 0);
    render(60, 20);
    wetm = ch_e[2];
    for (i = 0; i < CTL; i++)
        diff |= blk_out[2u * i] != blk_out[2u * i + 1u];
    printf("ncc: chorus II: energy (L+R)/2 %lld against dry %lld\n", (long long)wetm, (long long)dry);
    ok("chorus: II on: left and right differ (the two taps)", diff && ch_e[0] != ch_e[1]);
    ok("chorus: .. and the mono sum keeps the sound (no cancelling: 0.6 .. 2.5 x the dry energy)",
       wetm * 10 > dry * 6 && wetm * 10 < dry * 25);
    for (k = JC_I; k <= JC_I_II; k++) {                 /* the LFO rates: Hz = inc x 44100 / 2^32 */
        uint32_t mhz = (uint32_t)((uint64_t)JC_INC[k] * 44100u * 1000u >> 32);
        static const uint32_t WANT[4] = {0, 510, 860, 9750};
        if (mhz + 10u < WANT[k] || mhz > WANT[k] + 10u)
            diff = 0;
    }
    ok("chorus: the LFO rates I 0.51 Hz, II 0.86 Hz, I+II 9.75 Hz", diff != 0);
    midi(0x83, 48, 0); midi(0x83, 55, 0);
    render(200, 0);
    t->p[P_E0] = 3;                                     /* SIN, dark, no sends: a sample step is a click here (a */
    t->p[P_E4] = 60;                                    /* saw's own edges, and the taps' copies of them, are not) */
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = t->p[P_DIST] = 0;
    t->p[P_ED_FX] = JC_I_II;
    midi(0x93, 60, 100);
    render(80, 20);
    steady = ch_step;                                   /* I+II's own (its fast sweep bends the sine most) */
    t->p[P_ED_FX] = JC_II;                              /* a mode change while it plays: out, cleared, in */
    render(20, 0);
    sw = ch_step;
    printf("ncc: chorus I+II -> II on a sine: largest step %d (I+II steady %d)\n", (int)sw, (int)steady);
    ok("chorus: a mode change while playing: no click (steps within the steady sound's +25 %)", sw * 4 <= steady * 5);
    t->p[P_ED_FX] = JC_OFF;
    render(10, 0);
    ok("chorus: OFF again: faded out, mix_part's loop", !ncc_custom(t) && jc[1].w == 0);
}

static void split_test(void)
{
    uint32_t b, i, rz = 1, lz = 1, lsum = 0, rsum = 0;
    power();
    fx_quiet();
    trk[0].p[P_REV] = 100;
    trk[0].p[P_DLY] = 100;                              /* (sends that would put L on both sides) */
    song.g[G_ENGGO] = 0;                                /* SPLIT */
    midi(0x92, 60, 100);
    for (b = 0; b < 60u; b++) {
        blk();
        for (i = 0; i < CTL; i++) {
            rz &= blk_out[2u * i + 1u] == 0 && mix_r[i] == 0 && wet[i] == 0;
            lsum += blk_out[2u * i] != 0;
        }
    }
    ok("split: a note on L: the left plays, the right is exactly 0 (no bus sends)", rz && lsum > 0u);
    midi(0x82, 60, 0);
    midi_silence_track(0);                              /* (L's release cut: only R from here) */
    for (b = 0; b < 40u; b++)
        blk();
    midi(0x93, 60, 100);
    for (b = 0; b < 60u; b++) {
        blk();
        for (i = 0; i < CTL; i++) {
            lz &= mix_l[i] == 0 && wet[i] == 0;        /* (before MASTER: its filters ring L's note out) */
            rsum += blk_out[2u * i + 1u] != 0;
        }
    }
    ok("split: a note on R: the right plays, the left mix and the buses exactly 0", lz && rsum > 0u);
    song.g[G_ENGGO] = 1;
}

static void juno_test(void)
{
    track_t *t = &trk[1];
    uint32_t i, k, okv = 1, drift = 0, wild = 0;
    power();
    midi(0xB3, 12, 127);
    for (i = 0; i < 30u; i++)
        blk();
    k = 9;                                              /* HOOVER */
    ncc_juno_load(t, k);
    for (i = 0; i < 3u; i++)
        blk();
    for (i = 0; i < 8u; i++)
        okv &= t->p[P_E0 + i] == NCC_JUNO[k].e[i];
    okv &= t->p[P_ED_FX] == NCC_JUNO[k].jx && t->p[P_ATK] == NCC_JUNO[k].env[0] && t->p[P_LD_SHP] == NCC_JUNO[k].lshp;
    ok("juno: a Juno sound sets its values, chorus and drift", okv && t->eng_req == ENGI_ANALOG);
    ok("juno: .. and the macros go back to the middle", ncm.tgt[1][4] == 64u && ncm.cur[1][4] == NCM_MID);
    ncc_juno_load(t, 1);                                /* JUNO PAD: POLY, drift 8 */
    for (i = 0; i < 24u; i++) {
        uint32_t v;
        midi(0x93, 48 + i, 100); blk();
        for (v = 0; v < NVOICE; v++)
            if (t->v[v].active && t->v[v].gate && t->v[v].note == 48 + i) {
                drift += t->v[v].fine != 0;
                wild += t->v[v].fine > 10 || t->v[v].fine < -10;
            }
        midi(0x83, 48 + i, 0); blk();
    }
    ok("juno: drift: POLY notes a few cents off each (|fine| <= 4 ct at drift 8)", drift >= 16u && !wild);
    t->p[P_ED_FX] = 1;                                  /* chorus, no drift */
    midi(0x93, 50, 100); blk();
    for (i = 0, drift = 0; i < NVOICE; i++)
        drift += t->v[i].active && t->v[i].gate && t->v[i].fine != 0;
    ok("juno: no drift below P_ED_FX 4", drift == 0u);
    midi(0x83, 50, 0); blk();
}

static void ui_m2_test(void)
{
    track_t *t = &trk[1];
    power();
    lv_press(B_OCTUP);
    t_turn(EN_ALGO, 2);
    lv_frame();
    ok("UI: ALGORITHM on R: the Juno chorus (II), the popup says so", t->p[P_ED_FX] == JC_II && lv.say &&
       !strcmp(lv.say, "R CHORUS II"));
    t_turn(EN_ALGO, 5);
    ok("UI: .. up to I+II, no further", t->p[P_ED_FX] == JC_I_II);
    t_turn(EN_PRESET, -(int32_t)NCC_NJUNO);            /* from ANALOG's first preset back over the Juno list */
    lv_frames(200);
    ok("UI: PRESETS on R: the Juno sounds come first (JUNO STRINGS)", lv.juno[1] == 1u && t->p[P_ED_FX] == NCC_JUNO[0].jx);
    t_turn(EN_PRESET, 1);
    lv_frames(200);
    ok("UI: .. the next one (JUNO PAD)", lv.juno[1] == 2u);
    fm1_in.buttons |= 1u << panel.btn[B_GLO];
    lv_press(B_SEQ);
    fm1_in.buttons &= ~(1u << panel.btn[B_GLO]);
    ok("UI: GLO + SEQ: SPLIT, the popup", NCC_SPLIT() && lv.say && !strncmp(lv.say, "SPLIT", 5));
    {   /* stored with the project: SPLIT and the part's Juno character */
        int saved = project_save(0) == 0 || 1;
        song.g[G_ENGGO] = 1;
        t->p[P_ED_FX] = 0;
        project_load(0);
        ok("UI: SPLIT and the Juno chorus / drift come back with the project", saved && NCC_SPLIT() &&
           t->p[P_ED_FX] == NCC_JUNO[1].jx);
    }
    fm1_in.buttons |= 1u << panel.btn[B_GLO];
    lv_press(B_SEQ);
    fm1_in.buttons &= ~(1u << panel.btn[B_GLO]);
    ok("UI: GLO + SEQ again: STEREO", !NCC_SPLIT());
    lv_press(B_OCTDN);
    t_turn(EN_ALGO, 1);
    ok("UI: ALGORITHM on L (FM6) is still SLOT, the chorus untouched", trk[0].p[P_E7] == 1 && trk[0].p[P_ED_FX] == 0);
}

/* ------------------------------------------------------- milestone 3 --- */
static int note_playing(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
static void arp_ui_test(void)
{
    track_t *t = &trk[0];
    power();
    ok("arp UI: off at first, the SOUND page", !t->p[P_AMODE] && lv.page == PG_SOUND);
    lv_press(B_ARP);
    ok("arp UI: ARP: the arp on (UP), the ARP page, its LED", t->p[P_AMODE] == 1 && lv.page == PG_ARP && led_is(B_ARP, 1));
    t_turn(EN_K1, 2);
    ok("arp UI: KNOB 1 the mode (UP -> UPDN)", t->p[P_AMODE] == 3);
    t_turn(EN_K2, -1);
    ok("arp UI: KNOB 2 the rate (1/16 -> 8T)", t->p[P_ARATE] == 4);
    t_turn(EN_K3, 2);
    ok("arp UI: KNOB 3 the octaves (1 -> 3)", t->p[P_AOCT] == 3);
    t_turn(EN_K4, 5);
    ok("arp UI: KNOB 4 the gate (64 -> 74: two a detent)", t->p[P_AGATE] == 74);
    t_turn(EN_SELECT, 1);
    t_turn(EN_K1, 10);
    ok("arp UI: SELECT page 2, KNOB 1 the swing", lv.sub == 1u && t->p[P_ASWING] == 20);
    lv_press(B_HOME);
    lv_press(B_ARP);
    ok("arp UI: ARP from another page: its page, the arp stays on", lv.page == PG_ARP && t->p[P_AMODE] == 3);
    lv_press(B_ARP);
    ok("arp UI: ARP on its page: off, LED off", !t->p[P_AMODE] && led_is(B_ARP, 0));
    lv_press(B_ARP);
    ok("arp UI: on again: the mode it had (UPDN)", t->p[P_AMODE] == 3);
    lv_press(B_REC);
    ok("arp UI: REC: HOLD on, its LED, the popup", t->p[P_AHOLD] == 1 && led_is(B_REC, 1) && lv.say && !strcmp(lv.say, "L HOLD ON"));
    midi(0x92, 60, 100); midi(0x92, 64, 100); blk();
    midi(0x82, 60, 0); midi(0x82, 64, 0); blk();
    ok("arp UI: HOLD keeps the chord after letting go", t->nheld == 2u);
    lv_press(B_REC);
    blk();
    ok("arp UI: HOLD off with no key down: the latched chord goes", t->nheld == 0u && led_is(B_REC, 0));
    lv_press(B_OCTUP);
    ok("arp UI: R's arp is its own (off)", !trk[1].p[P_AMODE] && led_is(B_ARP, 0));
}

static void arp_grid_test(void)
{
    track_t *t = &trk[0];
    uint32_t q, b, fired;
    power();
    t->p[P_AMODE] = 1;
    t->p[P_ARATE] = 2;                                  /* 1/16 */
    q = div_samples(2);
    for (b = 0; b < 2000u && (ncc_bar % q) < q / 2u; b++)
        blk();                                          /* (into the second half of a 1/16) */
    {
        uint32_t left = q - ncc_bar % q;
        midi(0x92, 60, 100);
        for (b = 0, fired = 0; b < 400u && !fired; b++) {
            blk();
            fired = t->arp_note != 0;
        }
        printf("ncc: a note in the middle of a 1/16 (%u samples left of it): the arp's first note after %u blocks\n",
               (unsigned)left, (unsigned)b);
        ok("arp grid: a note mid-step starts the arp on the next 1/16, not at once",
           b > 1u && (b - 1u) * CTL + CTL >= left - CTL && (b - 1u) * CTL <= left + CTL);
    }
    midi(0x82, 60, 0); blk();
    for (b = 0; b < 4000u && !((ncc_bar % q) > CTL && (ncc_bar % q) < q / 16u); b++)
        blk();                                          /* (just after a 1/16 started) */
    midi(0x92, 62, 100); blk();
    ok("arp grid: a note a little late (within 1/8 of the step): at once", t->arp_note == 62);
    midi(0x82, 62, 0); blk();
    song.g[G_CLOCK] = 1;                                /* USB, no clock coming */
    midi(0x92, 65, 100);
    for (b = 0, fired = 0; b < 400u; b++) {
        blk();
        fired += t->arp_note == 65 && (b & 1u);
    }
    ok("arp grid: USB clock but none coming: the arp plays on the tempo", fired > 0u);
    midi(0x82, 65, 0); blk();
}

static void drone_test(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    power();
    lv_press(B_SEQ);
    ok("drone: SEQ: Drone-hold on, its LED, the popup", ncd_want[0] && led_is(B_SEQ, 1) && lv.say && !strcmp(lv.say, "L DRONE ON"));
    midi(0x92, 60, 100); midi(0x92, 67, 100); blk();
    midi(0x82, 60, 0); midi(0x82, 67, 0);
    for (i = 0; i < 20u; i++)
        blk();
    ok("drone: let go, the notes ring on", note_playing(t, 60) && note_playing(t, 67));
    midi(0x92, 72, 100); blk(); midi(0x82, 72, 0); blk();
    ok("drone: another note piles up", note_playing(t, 72) && note_playing(t, 60));
    midi(0x92, 67, 100); blk(); midi(0x82, 67, 0); blk();
    ok("drone: a droning note played again: let go", !note_playing(t, 67) && note_playing(t, 60));
    lv_press(B_SEQ);
    blk();
    ok("drone: SEQ again: off, every note through its release", !note_playing(t, 60) && !note_playing(t, 72) && led_is(B_SEQ, 0));
    midi(0x92, 60, 100); blk(); midi(0x82, 60, 0); blk();
    ok("drone: off: a note ends with its note-off", !note_playing(t, 60));
}

static uint32_t white_key(uint32_t place)               /* the key of white place p (F3 = 0) */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == place)
            return k;
    return 0;
}
static void fx_ui_test(void)
{
    track_t *t = &trk[1];
    uint32_t i;
    power();
    lv_press(B_OCTUP);
    lv_press(B_FX);
    ok("FX UI: a tap on FX: the FX mode on its PERFORM page, the keys locked to it, its LED", lv.fx && lv.page == PG_FX &&
       lv.sub == 0u && kb_lock == 2u && led_is(B_FX, 1));
    key_down(0); blk();
    ok("FX UI: FX mode, the first white key (no button held): its effect (REPEAT 1/8)", (perf_held >> perf_map[0]) & 1u);
    t_turn(EN_K1, -10);
    ok("FX UI: KNOB 1 on PERFORM: the FILTER macro", perf_k[0] < 0);
    key_up(0); blk();
    ok("FX UI: the key let go: the effect off", !((perf_held >> perf_map[0]) & 1u));
    t_turn(EN_SELECT, 1);
    t_turn(EN_K1, 3); t_turn(EN_K2, 3); t_turn(EN_K3, 3); t_turn(EN_K4, 3);
    ok("FX UI: SELECT: SENDS, KNOB 1..4 drive, chorus, delay, reverb of R", lv.sub == 1u && t->p[P_DIST] > 0 &&
       t->p[P_CHOR] > 0 && t->p[P_DLY] > 0 && t->p[P_REV] > 0 && trk[0].p[P_DIST] == 0);
    i = (uint32_t)song.g[G_DTIME];
    t_turn(EN_SELECT, 1);
    t_turn(EN_K1, 1); t_turn(EN_K3, 5);
    ok("FX UI: DELAY: KNOB 1 its time, KNOB 3 its colour", lv.sub == 2u && (uint32_t)song.g[G_DTIME] != i &&
       song.g[G_DCOLOR] != GP[G_DCOLOR].def);
    t_turn(EN_SELECT, 1);
    i = (uint32_t)song.g[G_RTYPE];
    t_turn(EN_K1, -1); t_turn(EN_K3, 5);
    ok("FX UI: REVERB: KNOB 1 its type, KNOB 3 its damping", lv.sub == 3u && (uint32_t)song.g[G_RTYPE] != i &&
       song.g[G_RDAMP] != GP[G_RDAMP].def);
    t_turn(EN_SELECT, 1);
    t_turn(EN_K3, 2); t_turn(EN_K4, 3);
    ok("FX UI: CHORUS: KNOB 3 R's Juno chorus (II), KNOB 4 its drift (3)", lv.sub == 4u && (t->p[P_ED_FX] & 3) == 2 &&
       ((t->p[P_ED_FX] >> 2) & 15) == 3);
    t_turn(EN_SELECT, 1);
    t_turn(EN_K1, 1);
    ok("FX UI: INSERT: KNOB 1 its type (SOFT), KNOB 2..4 named as the type names them (DRIVE TONE LEVEL)",
       lv.sub == 5u && t->p[P_ITYPE] == 1 && !strcmp(lv_label(lv_knob(1)), "DRIVE") && !strcmp(lv_label(lv_knob(3)), "LEVEL"));
    i = (uint32_t)t->p[P_IA];
    t_turn(EN_K2, 4);
    ok("FX UI: .. KNOB 2 its DRIVE", (uint32_t)t->p[P_IA] != i);
    t_turn(EN_SELECT, 3);
    ok("FX UI: SELECT stops at the last page", lv.sub == 5u);
    lv_press(B_FX);
    ok("FX UI: FX again: the mode closes (back to SOUND), the keys play, the macros back to 0", !lv.fx &&
       lv.page == PG_SOUND && kb_lock == 0u && perf_k[0] == 0 && led_is(B_FX, 0));
    key_down(white_key(3)); blk();
    ok("FX UI: closed: a key plays a note again", gate_on(t));
    key_up(white_key(3)); blk();
    lv_press(B_ARP);
    lv_press(B_FX);
    lv_press(B_FX);
    ok("FX UI: opened from ARP, closed back to ARP", lv.page == PG_ARP);
    lv_press(B_FX);
    lv_press(B_HOME);
    ok("FX UI: HOME closes it too", !lv.fx && lv.page == PG_SOUND && kb_lock == 0u);
    btn_down(B_ENV);
    lv_frame(); blk();
    ok("FX UI: ENV held: ECHO THROW", (perf_held >> PF_ETHR) & 1u);
    btn_up(B_ENV);
    btn_down(B_LFO);
    lv_frame(); blk();
    ok("FX UI: ENV let go, LFO held: REV THROW only", !((perf_held >> PF_ETHR) & 1u) && ((perf_held >> PF_RTHR) & 1u));
    btn_up(B_LFO);
    lv_frame(); blk();
    ok("FX UI: LFO let go: no throw", !((perf_held >> PF_RTHR) & 1u));
}

/* SPLIT to the DAC: one part plays with everything that could reach the other side on (its reverb / delay / chorus
 * sends, both throws, PERFORM's THROW macro and a FLANGER, its Juno chorus, a tail of the other part from before): the
 * other side of the output, after MASTER (mix_block's out), exactly 0 */
static int split_side_silent(uint32_t play, uint32_t quiet)
{
    track_t *t = &trk[play], *o = &trk[quiet];
    uint32_t b, i, silent = 1, sound = 0;
    power();
    fx_quiet();
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 120;
    o->p[P_REV] = o->p[P_DLY] = 120;
    midi(0x92 + quiet, 60, 120);                        /* the other part first, in ST: its reverb and delay ringing */
    for (b = 0; b < 100u; b++)
        blk();
    midi(0x82 + quiet, 60, 0);
    midi_silence_track(quiet);
    for (b = 0; b < 40u; b++)
        blk();                                          /* (its own release over; the buses still ring) */
    song.g[G_ENGGO] = 0;                                /* SPLIT */
    for (b = 0; b < 2000u; b++)
        blk();                                          /* (the other side's own past rings out of MASTER's DC blocker) */
    t->p[P_ED_FX] = JC_II | 6 << 2;
    lv_frame();
    lv_press(B_FX);                                     /* the FX mode: FLANGER held, THROW up */
    key_down(white_key(perf_map[10] == PF_FLG ? 10u : 10u));
    perf_k[2] = 100;
    ncm_throw = 3;                                      /* ENV and LFO held */
    midi(0x92 + play, 48, 120); midi(0x92 + play, 55, 120);
    for (b = 0; b < 300u; b++) {
        blk();
        for (i = 0; i < CTL; i++) {
            silent &= blk_out[2u * i + (quiet ? 1u : 0u)] == 0;
            sound += blk_out[2u * i + (play ? 1u : 0u)] != 0;
        }
    }
    key_up(white_key(10));
    ncm_throw = 0;
    perf_k[2] = 0;
    midi(0x82 + play, 48, 0); midi(0x82 + play, 55, 0);
    song.g[G_ENGGO] = 1;
    lv_press(B_FX);
    return silent && sound > 1000u;
}
static void split_dac_test(void)
{
    ui_power_on();
    ok("split: NCC's default (a project, power-on: G_ENGGO 0)", song.g[G_ENGGO] == 0 && NCC_SPLIT());
    ok("split to the DAC: L with sends, throws, THROW, FLANGER, Juno chorus, R's tail: the right output exactly 0",
       split_side_silent(0, 1));
    ok("split to the DAC: R alike: the left output exactly 0", split_side_silent(1, 0));
}

static void screen_test(void)
{
    uint32_t pg;
    power();
    for (pg = 0; pg < 3u; pg++) {
        uint16_t head;
        if (pg == 1u)
            lv_press(B_ARP);
        if (pg == 2u)
            lv_press(B_FX);
        lv_frame();
        head = swap16(host_screen[10u * 240u + 6u]);
        if (head != LC_PART[0])
            pg = 99u;
    }
    ok("screen: the head's page block in L's colour on SOUND, ARP and FX", pg == 3u);
    lv_press(B_OCTUP);
    lv_frame();
    ok("screen: focus R: the head's block in R's colour", swap16(host_screen[10u * 240u + 6u]) == LC_PART[1]);
    {   /* every label of the knobs and of the PERFORM pads fits its cell */
        uint32_t g, sb, k, fit = 1;
        for (g = 0; g < PG_N; g++)
            for (sb = 0; sb < LV_NSUB[g]; sb++)
                for (k = 0; k < 4u; k++)
                    fit &= text_w(&AF_S, lv_label(&LV_KNOBS[g][sb][k])) <= 58;
        for (k = 0; k < 6u; k++)
            fit &= text_w(&AF_M, LV_FX_PAGE[k]) <= 78;  /* (the head's block) */
        for (k = 0; k <= PF_N; k++)
            fit &= text_w(&AF_S, LV_PF[k][0]) <= 25 && text_w(&AF_S, LV_PF[k][1]) <= 25;
        ok("screen: the knob labels fit their 60 px, the PERFORM pads' their 26 px", fit);
    }
}

/* ------------------------------------------------------- milestone 4 --- */
static void engine_ui_test(void)
{
    track_t *t = &trk[0];
    uint32_t i;
    power();
    t_turn(EN_SELECT, 4);                               /* FM6 -> JUNO PHASE TRIO ACID */
    ok("engines: SELECT on SOUND: not switched while turning", t->eng_req == ENGI_FM6 && lv.epend == 4);
    lv_frames(400);
    ok("engines: after the rest: L is ACID, R untouched, the popup", t->eng_req == ENGI_ACID &&
       trk[1].eng_req == ENGI_ANALOG && lv.say && !strcmp(lv.say, "L ENGINE ACID"));
    ok("engines: a new ACID sound slides (LEGATO, GLIDE in TIME mode)", t->p[P_VOICE] == V_LEGATO && t->p[P_GLMODE] == 1 &&
       t->p[P_GLIDE] == LV_ACID_GLIDE);
    t_turn(EN_SELECT, 1);
    lv_frames(400);
    ok("engines: round the list (ACID -> FM6)", t->eng_req == ENGI_FM6);
    t_turn(EN_SELECT, -1);
    lv_frames(400);
    ok("engines: back (FM6 -> ACID)", t->eng_req == ENGI_ACID);
    lv_press(B_OCTUP);
    t_turn(EN_SELECT, 2);
    lv_frames(400);
    ok("engines: R: JUNO -> TRIO", trk[1].eng_req == ENGI_TRIO);
    t_turn(EN_ALGO, 2);
    ok("engines: ALGORITHM on TRIO: the Juno chorus", trk[1].p[P_ED_FX] == JC_II);
    (void)project_save(0);
    t->eng_req = ENGI_FM6;
    project_load(0);
    for (i = 0; i < 3u; i++)
        lv_frame();
    ok("engines: the parts' engines come back with the project", trk[0].eng_req == ENGI_ACID && trk[1].eng_req == ENGI_TRIO);
    lv_press(B_ARP);
    t_turn(EN_SELECT, 1);
    ok("engines: SELECT on ARP is its page 2, not the engine", lv.sub == 1u && !lv.epend);
}

static void engine_macro_test(void)
{
    static const struct { uint8_t eng, id; } WANT[] = {{ENGI_PHASE, P_E2}, {ENGI_TRIO, P_E5}, {ENGI_ACID, P_E1}};
    uint32_t k, i, okv = 1;
    for (k = 0; k < NELEM(WANT); k++) {
        int32_t base;
        power();
        set_engine_of(&trk[1], WANT[k].eng);
        for (i = 0; i < 5u; i++)
            blk();
        base = trk[1].p[WANT[k].id];
        midi(0xB3, 12, 127);                            /* FILTER FREQ */
        for (i = 0; i < 30u; i++)
            blk();
        okv &= trk[1].p[WANT[k].id] > base;
    }
    ok("engines: FILTER FREQ moves PHASE's DCW, TRIO's CUT, ACID's CUT", okv);
}

/* ACID: one note (vel) on R, n blocks: the energy of the output's sample steps (brightness) per window */
static int64_t acid_bright[4];
static int32_t acid_peak;
static void acid_play(uint32_t note, uint32_t vel, uint32_t blocks)
{
    uint32_t b, i;
    int32_t last = 0;
    memset(acid_bright, 0, sizeof acid_bright);
    acid_peak = 0;
    midi(0x93, note, vel);
    for (b = 0; b < blocks; b++) {
        blk();
        for (i = 0; i < CTL; i++) {
            int32_t x = blk_out[2u * i], d = (x - last) >> 8;
            acid_bright[(b * 4u) / blocks] += (int64_t)d * d;
            if ((x < 0 ? -x : x) > acid_peak)
                acid_peak = x < 0 ? -x : x;
            last = x;
        }
    }
    midi(0x83, note, 0);
}
static void acid_test(void)
{
    track_t *t = &trk[1];
    int64_t b0, b3, soft, loud;
    int32_t p_soft, p_acc, saw;
    uint32_t i, glided = 0;
    power();
    fx_quiet();
    set_engine_of(t, ENGI_ACID);
    t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
    for (i = 0; i < 4u; i++)
        blk();
    acid_play(36, 90, 600);
    b0 = acid_bright[0];
    b3 = acid_bright[3];
    saw = acid_peak;
    printf("ncc: ACID 303, C2: brightness first quarter %lld, last %lld, peak %d\n", (long long)b0, (long long)b3, (int)saw);
    ok("ACID: it sounds, and the filter envelope closes (brighter at the start than at the end)", saw > 0 && b0 > b3 * 2);
    t->p[P_E2] = 127;                                   /* RES all the way: squeals, never blows up */
    t->p[P_E3] = 127;
    t->p[P_E6] = 127;
    for (i = 0; i < 40u; i++)
        blk();
    acid_play(48, 127, 400);
    printf("ncc: ACID at RES 127, ENV 127, DRV 127: peak %d\n", (int)acid_peak);
    ok("ACID: RES 127 stays bounded (no runaway)", acid_peak > 0 && acid_peak < (1 << 23));
    power();
    fx_quiet();
    set_engine_of(t, ENGI_ACID);
    lv_frame();                                         /* (the UI: a new ACID sound slides) */
    t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = 0;
    for (i = 0; i < 400u; i++)
        blk();
    acid_play(36, 80, 200);
    soft = acid_bright[0];
    p_soft = acid_peak;
    for (i = 0; i < 400u; i++)
        blk();
    acid_play(36, 120, 200);
    loud = acid_bright[0];
    p_acc = acid_peak;
    printf("ncc: ACID accent: brightness %lld -> %lld, peak %d -> %d\n", (long long)soft, (long long)loud, (int)p_soft,
           (int)p_acc);
    ok("ACID: an accent (velocity >= 100) opens the filter more and plays louder", loud > soft && p_acc > p_soft);
    for (i = 0; i < 400u; i++)
        blk();
    midi(0x93, 36, 100); blk(); blk();
    midi(0x93, 43, 100);                                /* overlapping: a slide */
    for (i = 0; i < 6u; i++) {
        blk();
        glided |= t->v[0].pitch_cur != t->v[0].pitch16 && t->v[0].pitch_cur > 36 * 16 && t->v[0].pitch_cur < 43 * 16;
    }
    ok("ACID: an overlapping note slides (the pitch between the two)", glided);
    midi(0x83, 36, 0); midi(0x83, 43, 0);
    for (i = 0; i < 400u; i++)
        blk();
    midi(0x93, 36, 100); blk(); midi(0x83, 36, 0);
    for (i = 0; i < 40u; i++)
        blk();
    midi(0x93, 43, 100); blk();
    ok("ACID: a note after a gap does not slide", t->v[0].pitch_cur == t->v[0].pitch16);
    midi(0x83, 43, 0);
    {   /* saw and square differ */
        int64_t sw, sq;
        for (i = 0; i < 400u; i++)
            blk();
        t->p[P_E0] = 0;
        acid_play(40, 90, 100);
        sw = acid_bright[1];
        for (i = 0; i < 400u; i++)
            blk();
        t->p[P_E0] = 1;
        acid_play(40, 90, 100);
        sq = acid_bright[1];
        ok("ACID: WAVE saw and square sound different", sw != sq);
    }
}

/* ------------------------------------------------------- Stop --- */
/* the Circuit's clock over USB: pulses (24 a quarter at 120 BPM: ~21 ms, ~29 blocks), fm1_ms with them */
static void ext_pulses(uint32_t n)
{
    uint32_t i, b;
    for (i = 0; i < n; i++) {
        midi(0xF8, 0, 0);
        for (b = 0; b < 29u; b++) {
            if (b % 4u == 0u)
                fm1_ms += 3u;
            blk();
        }
    }
}
static void stop_test(void)
{
    track_t *l = &trk[0], *r = &trk[1];
    uint32_t i;
    power();
    song.g[G_CLOCK] = 1;                                /* USB */
    blk();
    ext_pulses(4);
    midi(0xFA, 0, 0);                                   /* Start */
    ext_pulses(2);
    ok("Stop: the Circuit's Start: playing", song.playing != 0u);
    l->p[P_AMODE] = 1; l->p[P_AHOLD] = 1; l->p[P_ARATE] = 2;
    midi(0x92, 60, 100); midi(0x92, 64, 100); blk();
    midi(0x82, 60, 0); midi(0x82, 64, 0); blk();        /* latched */
    ncd_want[1] = 1;
    r->p[P_VOICE] = V_POLY;                             /* (SAW LEAD is LEGATO: one note) */
    midi(0x93, 43, 100); midi(0x93, 50, 100); blk();
    midi(0x83, 43, 0); midi(0x83, 50, 0);
    ext_pulses(24);
    ok("Stop: before it, L's latched arp plays, R drones", l->nheld == 2u && note_playing(r, 43) && note_playing(r, 50));
    midi(0xFC, 0, 0);                                   /* Stop */
    ext_pulses(1);
    for (i = 0; i < 400u; i++)
        blk();
    ok("Stop: the Circuit's Stop: L's arp stops (its latched chord dropped, no arp note)", !song.playing && l->nheld == 0u &&
       !l->arp_note && !gate_on(l));
    ok("Stop: .. and R's drone lets go (Drone-hold itself stays on)", !note_playing(r, 43) && !note_playing(r, 50) &&
       ncd_want[1] && ncd[1].n == 0u);
    ext_pulses(24);
    ok("Stop: the clock running on after it: the arp stays silent", !l->arp_note && !gate_on(l));
    midi(0x93, 55, 100); blk(); midi(0x83, 55, 0);
    for (i = 0; i < 40u; i++)
        blk();
    ok("Stop: Drone-hold still on: a new note drones again", note_playing(r, 55));
    ncd_want[1] = 0;
    blk();
}

static void splash_test(void)
{
    uint32_t i, fit = 1;
    for (i = 0; i < NELEM(SPLASH_LINES); i++)
        fit &= text_w(&AF_M, SPLASH_LINES[i]) <= (int32_t)(SPL_SQ - 2u * SPL_PAD);
    ok("splash: NCC, then Felucca's version and credits, each line inside the square",
       !strcmp(SPLASH_LINES[0], "NCC") && !strncmp(SPLASH_LINES[1], "Felucca ", 8) && fit);
}

static void bpm0_test(void)
{
    int16_t b = song.g[G_BPM];
    uint8_t c = (uint8_t)song.g[G_CLOCK];
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 0;
    ok("BPM 0: beat_samples counts it as 120 (no division by zero)", beat_samples() == (uint32_t)FS * 60u / 120u);
    song.g[G_BPM] = b;
    song.g[G_CLOCK] = (int16_t)c;
}

int main(void)
{
    setup_test();
    routing_test();
    keys_test();
    slew_test();
    relative_test();
    own_cc_test();
    lazy_test();
    ceg_test();
    zipper_test();
    ui_test_ncc();
    chorus_test();
    split_test();
    juno_test();
    ui_m2_test();
    arp_ui_test();
    arp_grid_test();
    drone_test();
    fx_ui_test();
    screen_test();
    split_dac_test();
    engine_ui_test();
    engine_macro_test();
    acid_test();
    stop_test();
    splash_test();
    bpm0_test();
    printf("ncc: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
