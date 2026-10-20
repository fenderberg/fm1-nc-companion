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
 *   CEG        CC 5 on FM6: the carriers' envelope rates move, the modulators' do not
 *   ZIPPER     a cutoff jump by CC: the output's largest sample step stays near a steady note's
 *   UI         OCT- / OCT+ the focus and its LED; PRESETS loads once after a pause, on the focused part only;
 *              ALGORITHM steps FM6's SLOT; the popup with the knob's legend for LV_POP_MS
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
    ceg_test();
    zipper_test();
    ui_test_ncc();
    bpm0_test();
    printf("ncc: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
