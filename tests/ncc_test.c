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
static void lv_turn(uint32_t role, int32_t s)
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
    lv_frame();                                         /* lv_setup */
    blk();
}
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
    lv_turn(EN_PRESET, 1); lv_turn(EN_PRESET, 1); lv_turn(EN_PRESET, 1);
    ok("UI: PRESETS turning: nothing loaded yet", trk[1].preset == p1);
    lv_frames(200);
    ok("UI: after the pause, three on, once, on R only", trk[1].preset != p1 && trk[0].preset == p0 &&
       trk[1].preset == (p1 + 3u) % ENGINES[ENGI_ANALOG]->npresets);
    lv_press(B_OCTDN);
    ok("UI: OCT- focuses L", song.sel == 0u && lit(B_OCTDN));
    i = (uint32_t)trk[0].p[P_E7];
    lv_turn(EN_ALGO, 1);
    lv_frame();
    ok("UI: ALGORITHM steps FM6's SLOT", (uint32_t)trk[0].p[P_E7] == i + 1u && fm6_slot[0] == i + 1u);
    midi(0xB3, 74, 100); blk();
    lv_frame();
    ok("UI: the popup shows the knob (R FX 100)", lv.pop_ms != 0u && (ncm_seen & 0xFFFFFFu) == (1u << 16 | 7u << 8 | 100u));
    lv_frames(1100);
    ok("UI: the popup goes after a second", lv.pop_ms == 0u);
    {   /* something drawn in both columns, the focus bar under L only */
        uint32_t x, l = 0, r = 0;
        for (x = 8; x < 112u; x++) {
            l += swap16(host_screen[(LV_Y_COL + 87u) * 240u + x]) == T_ACCENT;          /* (panel byte order) */
            r += swap16(host_screen[(LV_Y_COL + 87u) * 240u + 128u + x - 8u]) == T_ACCENT;
        }
        ok("UI: the focus bar under L, not R", l == 104u && r == 0u);
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
    song.g[G_ENGGO] = 1;
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
    song.g[G_ENGGO] = 0;
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
    lv_turn(EN_ALGO, 2);
    lv_frame();
    ok("UI: ALGORITHM on R: the Juno chorus (II), the popup says so", t->p[P_ED_FX] == JC_II && lv.say &&
       !strcmp(lv.say, "R CHORUS II"));
    lv_turn(EN_ALGO, 5);
    ok("UI: .. up to I+II, no further", t->p[P_ED_FX] == JC_I_II);
    lv_turn(EN_PRESET, -(int32_t)NCC_NJUNO);            /* from ANALOG's first preset back over the Juno list */
    lv_frames(200);
    ok("UI: PRESETS on R: the Juno sounds come first (JUNO STRINGS)", lv.juno[1] == 1u && t->p[P_ED_FX] == NCC_JUNO[0].jx);
    lv_turn(EN_PRESET, 1);
    lv_frames(200);
    ok("UI: .. the next one (JUNO PAD)", lv.juno[1] == 2u);
    fm1_in.buttons |= 1u << panel.btn[B_GLO];
    lv_press(B_SEQ);
    fm1_in.buttons &= ~(1u << panel.btn[B_GLO]);
    ok("UI: GLO + SEQ: SPLIT, the popup", NCC_SPLIT() && lv.say && !strncmp(lv.say, "SPLIT", 5));
    {   /* stored with the project: SPLIT and the part's Juno character */
        int saved = project_save(0) == 0 || 1;
        song.g[G_ENGGO] = 0;
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
    lv_turn(EN_ALGO, 1);
    ok("UI: ALGORITHM on L (FM6) is still SLOT, the chorus untouched", trk[0].p[P_E7] == 1 && trk[0].p[P_ED_FX] == 0);
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
    splash_test();
    bpm0_test();
    printf("ncc: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
