/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of NCC (FELUCCA_LIVEUI=1, milestone 1) with the real sources: the macro layer (src/perf_layer.c, the
 * Circuit Tracks' knobs on CC 1 2 5 11 71 74 12 13), FM6's AMP ENV (CEG), the live UI (src/live_ui.c) on the stub panel
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
 *              ALGORITHM L's engine filter; the popup with the knob's legend for LV_POP_MS
 *   CHORUS     the Juno chorus: OFF is Felucca's loop, II stereo (left != right, the mono sum kept), the rates, no
 *              click at a mode change, OFF fades out
 *   SPLIT      a note on L (no sends): the right output exactly 0, and the other way round; sends: their return on
 *              both sides
 *   JUNO       a Juno sound's values, chorus and drift; the macros back to 64; the drift per POLY note, none below 4
 *   UI 2       PRESETS on R: the Juno sounds before ANALOG's, GLO + SEQ SPLIT, both stored
 *   ARP UI     ARP on (its page, the mode back), KNOB 1..4 and page 2, off on its page; REC HOLD (and off lets go)
 *   ARP GRID   a note mid-step starts the arp on the next step of the bar, a little late at once; no clock: BPM
 *   DRONE      SEQ: note-offs ignored, notes pile up, a droning note again lets go, SEQ off releases all
 *   FX UI      FX opens its mode on SENDS (the keys PERFORM's, no button held), FX taps and SELECT its pages SENDS DELAY
 *              REVERB (+ R's send) CHORUS INSERT PERFORM (the insert's knobs named by its type), FX after the last /
 *              HOME close it; ENV / LFO throws
 *   SPLIT DAC  one part with FLANGER and Juno chorus, no sends: the other side of the output after MASTER exactly 0;
 *              with sends, throws, THROW: their return on the other side too
 *   SCREEN     the head's block in the focused part's colour; the labels fit their cells
 *   LOOP       NCC's drum loop engine: a note of any pitch opens it, in time where the bar is (mid-bar too), a bar at
 *              90 BPM; SHUF REV STUT CRSH PTCH FILT change it; every loop plays, none clipping
 *   USER LOOPS a user slot's zones as LOOP sounds (the library, BEAT, the slot's name), decoded in the main loop into
 *              the part's buffer, in time on the beats at 120 and 90 BPM, SHUF on them, an absent zone silent, an
 *              upload decoded again, an emptied slot gone
 *   EDITOR     the editor protocol's NCC commands (editor_ncc.c): INFO, the library page by page, a sound on a part,
 *              the parts, the settings (SPLIT / STEREO, channel, filters), bad arguments
 *   NCC SOUNDS each of NCC's own sounds (ncc_presets.c) plays, none silent, none clipping
 *   SOUNDS     the library (ncc_library.c): every factory sound in a category; SELECT the category, ALGORITHM the engine
 *              (after a rest, past the empty ones, the sound kept if it fits), PRESETS through what fits and over the
 *              engines, a bank voice by its name, ACID slides, the engines stored; the Circuit's FILTER FREQ on PHASE,
 *              TRIO, ACID
 *   MACROS     every engine's 8 knobs (CC 0 / 127) move what NCM_MAP says; the popup names it with the value the
 *              parameter has now, fits beside the legend; ANALOG SUB / SYNC named so; FM6's vibrato a little
 *   ACID       it sounds, the filter envelope closes, RES 127 bounded, accent brighter and louder, slide on overlap
 *              only, saw / square
 *   BAR LOCK   on the external clock NCC's bar is the Circuit's, counted in its pulses: 16 bars of jittery pulses, no drift
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

/* the editor's NCC commands (editor_ncc.c) on their own: its reply bytes into ed_out (editor.c's helpers, here) */
static uint8_t ed_out[664];
static uint32_t ed_n;
static void ed_b(uint32_t v) { if (ed_n < sizeof ed_out) ed_out[ed_n++] = (uint8_t)(v & 127u); }
static void ed_str(const char *s, uint32_t max)
{
    uint32_t i;
    for (i = 0; s[i] && i < max; i++)
        ed_b((uint8_t)s[i]);
    ed_b(0);
}
static int32_t ed_rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }
#include "../firmware/src/editor_ncc.c"

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
    midi(0xB3, 71, 127);                                /* FILTER FREQ on R */
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
        midi(0xB3, 71, (i * 37u) & 127u);
    midi(0xB3, 71, 64);
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
    midi(0xB3, 74, 127);                                /* RESONANCE up */
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
    midi(0xB3, 71, 0);
    for (i = 0; i < 30u; i++)
        blk();
    ok("relative: clamped to the range (CUT 10, turned all the way down: 0)", t->p[P_E4] == 0);
    {                                                   /* the Circuit's FX up, then the panel turns DRIVE down */
        int32_t up;
        midi(0xB3, 13, 127);
        for (i = 0; i < 30u; i++)
            blk();
        up = t->p[P_DIST];
        t->p[P_DIST] = (int16_t)(up - 5);
        for (i = 0; i < 30u; i++)
            blk();
        ok("relative: a knob off the middle: the panel's value stays (DRIVE turned down stays down)",
           up > 0 && t->p[P_DIST] == up - 5);
        midi(0xB3, 13, 64);
        for (i = 0; i < 30u; i++)
            blk();
        ok("relative: .. the knob back to the middle: below it by the knob's turn", t->p[P_DIST] < up - 5);
    }
    midi(0xB2, 71, 0);                                  /* FILTER FREQ on L: MLVL -64 .. 63 */
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
            midi(0xB3, 71, 127);
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
    i = trk[0].preset;
    t_turn(EN_ALGO, 1);
    lv_frames(400);
    ok("UI: ALGORITHM: L's engine filter FM6, its FM6 sound kept", lv.ep[0] == 1u && trk[0].preset == i &&
       trk[0].eng_req == ENGI_FM6);
    lv.ep[0] = 0;
    midi(0xB3, 13, 100); blk();
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
    for (i = 0; i < 2u; i++)
        trk[i].p[P_REV] = trk[i].p[P_DLY] = trk[i].p[P_CHOR] = 0;
    song.g[G_ENGGO] = 0;                                /* SPLIT (no sends) */
    midi(0x92, 60, 100);
    for (b = 0; b < 60u; b++) {
        blk();
        for (i = 0; i < CTL; i++) {
            rz &= blk_out[2u * i + 1u] == 0 && mix_r[i] == 0 && wet[i] == 0;
            lsum += blk_out[2u * i] != 0;
        }
    }
    ok("split: a note on L: the left plays, the right is exactly 0 (no sends)", rz && lsum > 0u);
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
    midi(0x83, 60, 0);
    trk[1].p[P_REV] = trk[1].p[P_DLY] = 100;            /* sends: R's reverb and delay on both sides */
    midi(0x93, 62, 100);
    for (b = 0, lz = 0; b < 200u; b++) {
        blk();
        for (i = 0; i < CTL; i++)
            lz += wet[i] != 0;                          /* (mix_l: HALL's stereo side is added to it) */
    }
    ok("split: R with sends: its delay and reverb come back (on both sides)", lz > 100u);
    song.g[G_ENGGO] = 1;
}

static void juno_test(void)
{
    track_t *t = &trk[1];
    uint32_t i, k, okv = 1, drift = 0, wild = 0;
    power();
    midi(0xB3, 71, 127);
    for (i = 0; i < 30u; i++)
        blk();
    k = 9;                                              /* HOOVER */
    ncc_snd_load(t, k);
    for (i = 0; i < 3u; i++)
        blk();
    for (i = 0; i < 8u; i++)
        okv &= t->p[P_E0 + i] == NCC_SND[k].e[i];
    okv &= t->p[P_ED_FX] == NCC_SND[k].jx && t->p[P_ATK] == NCC_SND[k].env[0] && t->p[P_LD_SHP] == NCC_SND[k].lshp;
    ok("juno: a Juno sound sets its values, chorus and drift", okv && t->eng_req == ENGI_ANALOG);
    ok("juno: .. and the macros go back to the middle", ncm.tgt[1][4] == 64u && ncm.cur[1][4] == NCM_MID);
    ncc_snd_load(t, 1);                                /* JUNO PAD: POLY, drift 8 */
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

static uint32_t ncc_count(uint32_t e)                 /* NCC's own sounds of engine e (ncc_presets.c) */
{
    uint32_t k, n = 0;
    for (k = 0; k < NCC_NSND; k++)
        n += NCC_SND[k].eng == e;
    return n;
}
static void ui_m2_test(void)
{
    track_t *t = &trk[1];
    power();
    lv_press(B_OCTUP);
    t_turn(EN_PRESET, -(int32_t)ncc_count(ENGI_ANALOG));   /* from ANALOG's first preset back over NCC's own */
    lv_frames(200);
    ok("UI: PRESETS on R: the Juno sounds come first (JUNO STRINGS)", lv.juno[1] == 1u && t->p[P_ED_FX] == NCC_SND[0].jx);
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
           t->p[P_ED_FX] == NCC_SND[1].jx);
    }
    fm1_in.buttons |= 1u << panel.btn[B_GLO];
    lv_press(B_SEQ);
    fm1_in.buttons &= ~(1u << panel.btn[B_GLO]);
    ok("UI: GLO + SEQ again: STEREO", !NCC_SPLIT());
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
    ok("FX UI: a tap on FX: the FX mode on its SENDS page, the keys locked to it, its LED", lv.fx && lv.page == PG_FX &&
       lv.sub == 0u && kb_lock == 2u && led_is(B_FX, 1));
    key_down(0); blk();
    ok("FX UI: FX mode, the first white key (no button held): its effect (REPEAT 1/8)", (perf_held >> perf_map[0]) & 1u);
    key_up(0); blk();
    ok("FX UI: the key let go: the effect off", !((perf_held >> perf_map[0]) & 1u));
    t_turn(EN_K1, 3); t_turn(EN_K2, 3); t_turn(EN_K3, 3); t_turn(EN_K4, 3);
    ok("FX UI: SENDS: KNOB 1..4 drive, chorus, delay, reverb of R", t->p[P_DIST] > 0 && t->p[P_CHOR] > 0 &&
       t->p[P_DLY] > 0 && t->p[P_REV] > 0 && trk[0].p[P_DIST] == 0);
    i = (uint32_t)song.g[G_DTIME];
    lv_press(B_FX);
    t_turn(EN_K1, 1); t_turn(EN_K3, 5);
    ok("FX UI: FX again: DELAY, KNOB 1 its time, KNOB 3 its colour", lv.fx && lv.sub == 1u &&
       (uint32_t)song.g[G_DTIME] != i && song.g[G_DCOLOR] != GP[G_DCOLOR].def);
    t_turn(EN_SELECT, 1);
    i = (uint32_t)song.g[G_RTYPE];
    {
        int32_t rev = t->p[P_REV];
        t_turn(EN_K1, -1); t_turn(EN_K3, 5); t_turn(EN_K4, 3);
        ok("FX UI: SELECT: REVERB, KNOB 1 its type, KNOB 3 its damping, KNOB 4 R's reverb send", lv.sub == 2u &&
           (uint32_t)song.g[G_RTYPE] != i && song.g[G_RDAMP] != GP[G_RDAMP].def && t->p[P_REV] > rev);
    }
    lv_press(B_FX);
    t_turn(EN_K3, 2); t_turn(EN_K4, 3);
    ok("FX UI: CHORUS: KNOB 3 R's Juno chorus (II), KNOB 4 its drift (3)", lv.sub == 3u && (t->p[P_ED_FX] & 3) == 2 &&
       ((t->p[P_ED_FX] >> 2) & 15) == 3);
    t_turn(EN_SELECT, 1);
    t_turn(EN_K1, 1);
    ok("FX UI: INSERT: KNOB 1 its type (SOFT), KNOB 2..4 named as the type names them (DRIVE TONE LEVEL)",
       lv.sub == 4u && t->p[P_ITYPE] == 1 && !strcmp(lv_label(lv_knob(1)), "DRIVE") && !strcmp(lv_label(lv_knob(3)), "LEVEL"));
    i = (uint32_t)t->p[P_IA];
    t_turn(EN_K2, 4);
    ok("FX UI: .. KNOB 2 its DRIVE", (uint32_t)t->p[P_IA] != i);
    lv_press(B_FX);
    t_turn(EN_K1, -10);
    ok("FX UI: PERFORM, the last page: KNOB 1 the FILTER macro", lv.sub == LV_FX_PERF && perf_k[0] < 0);
    t_turn(EN_SELECT, 3);
    ok("FX UI: SELECT stops at the last page", lv.sub == LV_FX_PERF);
    lv_press(B_FX);
    ok("FX UI: FX on the last page: the mode closes (back to SOUND), the keys play, the macros back to 0", !lv.fx &&
       lv.page == PG_SOUND && kb_lock == 0u && perf_k[0] == 0 && led_is(B_FX, 0));
    key_down(white_key(3)); blk();
    ok("FX UI: closed: a key plays a note again", gate_on(t));
    key_up(white_key(3)); blk();
    lv_press(B_ARP);
    for (i = 0; i <= LV_FX_PERF + 1u; i++)
        lv_press(B_FX);
    ok("FX UI: opened from ARP, tapped through, closed back to ARP", !lv.fx && lv.page == PG_ARP);
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
/* wet 0: no sends, no throws: the quiet side exactly 0; wet 1: sends, throws, THROW: their return on it too */
static int split_side_silent(uint32_t play, uint32_t quiet, uint32_t wet)
{
    track_t *t = &trk[play], *o = &trk[quiet];
    uint32_t b, i, silent = 1, sound = 0;
    power();
    fx_quiet();
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = (int16_t)(wet ? 120 : 0);
    o->p[P_REV] = o->p[P_DLY] = (int16_t)(wet ? 120 : 0);
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
    perf_k[2] = (int16_t)(wet ? 100 : 0);
    ncm_throw = (uint8_t)(wet ? 3 : 0);                 /* ENV and LFO held */
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
    return (wet ? !silent : silent) && sound > 1000u;
}
static void split_dac_test(void)
{
    ui_power_on();
    ok("split: NCC's default (a project, power-on: G_ENGGO 0)", song.g[G_ENGGO] == 0 && NCC_SPLIT());
    ok("split to the DAC: L with FLANGER, Juno chorus, no sends: the right output exactly 0", split_side_silent(0, 1, 0));
    ok("split to the DAC: R alike: the left output exactly 0", split_side_silent(1, 0, 0));
    ok("split to the DAC: L with sends, throws, THROW: their return on the right too", split_side_silent(0, 1, 1));
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
/* LOOP: R plays loop preset lp with p's E values changed by set (id, value pairs, -1 ends), a note (pitch) opened
 * at sample `from` of a bar for `len` samples; out: the right output of a bar and a half (from the bar's start) */
static int32_t loop_out[132300];
static void loop_play(uint32_t lp, const int16_t *set, uint32_t note, uint32_t from, uint32_t len, uint32_t bpm)
{
    uint32_t b, i, at, nb = 132300u / CTL + 2u;
    power();
    fx_quiet();
    song.g[G_BPM] = (int16_t)bpm;
    set_engine_of(&trk[1], ENGI_LOOP);
    apply_preset_to(&trk[1], lp);
    trk[1].p[P_DLY] = trk[1].p[P_REV] = trk[1].p[P_CHOR] = trk[1].p[P_DIST] = 0;
    for (; set && set[0] >= 0; set += 2)
        trk[1].p[set[0]] = set[1];
    for (i = 0; i < 24u; i++)
        loop_user_poll();                               /* (a user loop: decoded, as the main loop would) */
    lv_frame();
    memset(loop_out, 0, sizeof loop_out);
    while (ncc_bar % (4u * beat_samples()) > CTL)       /* (to the start of a bar) */
        blk();
    at = ncc_bar % (4u * beat_samples());               /* (kept by the bar's place: every run alike) */
    for (b = 0; b < nb; b++, at += CTL) {
        if (from + CTL > at && from <= at)
            midi(0x93, note, 100);
        if (from + len + CTL > at && from + len <= at)
            midi(0x83, note, 0);
        blk();
        for (i = 0; i < CTL; i++)
            if (at + i < 132300u)
                loop_out[at + i] = blk_out[2u * i + 1u];
    }
}
static int64_t loop_energy(uint32_t from, uint32_t to)
{
    int64_t e = 0;
    for (; from < to; from++)
        e += (int64_t)(loop_out[from] >> 4) * (loop_out[from] >> 4);
    return e;
}
static uint64_t loop_hash(uint32_t from, uint32_t to)
{
    uint64_t h = 1469598103934665603ull;
    for (; from < to; from++)
        h = (h ^ (uint32_t)loop_out[from]) * 1099511628211ull;
    return h;
}
/* the share (of 1000) of the bar's samples that are not as ref's (beyond two runs' small MASTER differences) */
static uint32_t loop_unlike(const int32_t *ref, uint32_t bar)
{
    uint32_t i, far = 0;
    for (i = 64; i < bar; i++) {
        int32_t d = loop_out[i] - ref[i];
        far += (d < 0 ? -d : d) > ((ref[i] < 0 ? -ref[i] : ref[i]) >> 3) + 1024;
    }
    return far * 1000u / (bar - 64u);
}
/* the energy of (out - ref) against ref's, per 1000 */
static uint32_t loop_err(const int32_t *ref, uint32_t bar)
{
    int64_t e = 0, r = 0;
    uint32_t i;
    for (i = 64; i < bar; i++) {
        int32_t d = (loop_out[i] - ref[i]) >> 4, q = ref[i] >> 4;
        e += (int64_t)d * d;
        r += (int64_t)q * q;
    }
    return r ? (uint32_t)(e * 1000 / r) : 0u;
}
static void loop_test(void)
{
    static int32_t ref[132300];
    static const int16_t SHUF[] = {P_E2, 127, -1}, REV[] = {P_E4, 127, -1}, STUT[] = {P_E7, 100, -1},
                         CRSH[] = {P_E6, 110, -1}, FILT[] = {P_E5, -60, -1}, PTCH[] = {P_E1, 7, -1};
    uint32_t bar = 88200u, i, near = 0, n = 0;
    int64_t e0, ehi0 = 0, ehi1 = 0;
    loop_play(0, 0, 60, 0, 0, 120);
    ok("LOOP: no note, no sound", loop_energy(0, 132300) == 0);
    loop_play(0, 0, 60, 0, bar + bar / 2, 120);
    e0 = loop_energy(0, bar);
    memcpy(ref, loop_out, sizeof ref);
    ok("LOOP: a note opens it (JUNGLE BREAK, a bar)", e0 > 0);
    loop_play(0, 0, 37, 0, bar + bar / 2, 120);
    for (i = 64; i < bar; i++) {                        /* (as alike as two runs of one note: MASTER's state) */
        int32_t d = loop_out[i] - ref[i];
        near += (d < 0 ? -d : d) <= ((ref[i] < 0 ? -ref[i] : ref[i]) >> 3) + 1024;
        n++;
    }
    ok("LOOP: any note, any pitch: the same loop (the Circuit's scale does not matter)", near * 100u >= n * 99u);
    near = n = 0;
    loop_play(0, 0, 60, bar / 2 + 1000, bar, 120);      /* opened mid-bar: the second half as it is in the bar */
    for (i = bar / 2 + 1200; i < bar; i++) {
        int32_t d = loop_out[i] - ref[i];
        near += (d < 0 ? -d : d) <= ((ref[i] < 0 ? -ref[i] : ref[i]) >> 3) + 64;
        n++;
    }
    printf("ncc: LOOP: opened mid-bar, %u of %u samples as the full bar's\n", near, n);
    ok("LOOP: a note mid-bar plays the loop where the bar is (in time, no waiting)", near * 10u >= n * 9u &&
       loop_energy(0, bar / 2 + 1000) == 0);
    loop_play(0, 0, 60, 0, bar * 2u, 90);                /* 90 BPM: a bar is 117600 samples, the loop still fits */
    {
        int64_t a = loop_energy(0, 117600u / 4u), b2 = loop_energy(117600u, 132300u);
        ok("LOOP: at 90 BPM the bar still starts with the loop's start (its first hit again at the next bar)",
           a > 0 && b2 > 0);
    }
    loop_play(0, SHUF, 60, 0, bar, 120);
    ok("LOOP: SHUF 127 shuffles the slices", loop_unlike(ref, bar) > 100u && loop_energy(0, bar) > 0);
    loop_play(0, REV, 60, 0, bar, 120);
    ok("LOOP: REV 127 plays slices backwards", loop_unlike(ref, bar) > 100u && loop_energy(0, bar) > 0);
    loop_play(0, STUT, 60, 0, bar, 120);
    ok("LOOP: STUT 100 (beat repeat 1/32) repeats", loop_unlike(ref, bar) > 100u && loop_energy(0, bar) > 0);
    loop_play(0, CRSH, 60, 0, bar, 120);
    printf("ncc: LOOP: CRSH 110, error energy %u / 1000\n", loop_err(ref, bar));
    ok("LOOP: CRSH crushes", loop_err(ref, bar) > 50u && loop_energy(0, bar) > 0);
    loop_play(0, PTCH, 60, 0, bar, 120);
    ok("LOOP: PTCH +7 plays the slices faster", loop_unlike(ref, bar) > 100u && loop_energy(0, bar) > 0);
    for (i = 1; i < bar; i++) {
        int32_t d = ref[i] - ref[i - 1];
        ehi0 += (int64_t)(d >> 4) * (d >> 4);
    }
    loop_play(0, FILT, 60, 0, bar, 120);
    for (i = 1; i < bar; i++) {
        int32_t d = loop_out[i] - loop_out[i - 1];
        ehi1 += (int64_t)(d >> 4) * (d >> 4);
    }
    ok("LOOP: FILT -60 (the low-pass) takes the highs off", ehi1 * 4 < ehi0 && loop_energy(0, bar) > 0);
    {
        uint32_t k, peakok = 1;
        for (k = 0; k < NLOOP; k++) {
            int32_t pk = 0;
            loop_play(k, 0, 60, 0, bar, 120);
            for (i = 0; i < bar; i++)
                pk = loop_out[i] > pk ? loop_out[i] : -loop_out[i] > pk ? -loop_out[i] : pk;
            printf("ncc: LOOP %-16s peak %d\n", NLOOP_NAME[k], pk);
            peakok &= pk > 2000 && pk < 32000;
        }
        ok("LOOP: every loop plays, none clipping", peakok);
    }
}

/* a user slot of loops for LOOP: zone z (of nz) a bar at 22050 Hz (120 BPM: 44100 samples) with a click (a 300-sample
 * burst) every (z ? 16th : quarter); named name. IMA ADPCM as the web editor uploads it */
static void ulp_ima(const int16_t *x, uint32_t n, uint8_t *out)
{
    int32_t pred = 0, idx = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t diff = x[i] - pred, st = IMA_STEP[idx], vd = st >> 3;
        uint32_t code = 0;
        if (diff < 0) {
            code = 8;
            diff = -diff;
        }
        if (diff >= st) {
            code |= 4;
            diff -= st;
            vd += st;
        }
        if (diff >= st >> 1) {
            code |= 2;
            diff -= st >> 1;
            vd += st >> 1;
        }
        if (diff >= st >> 2) {
            code |= 1;
            vd += st >> 2;
        }
        pred = clamp(pred + ((code & 8) ? -vd : vd), -32768, 32767);
        idx = clamp(idx + IMA_IDX[code & 7], 0, 88);
        if (i & 1)
            out[i >> 1] |= (uint8_t)(code << 4);
        else
            out[i >> 1] = (uint8_t)code;
    }
}
static void ulp_slot(uint32_t k, const char *name, uint32_t nz)
{
    static int16_t x[44100];
    smp_user_hdr_t *h = (smp_user_hdr_t *)((uint8_t *)host_slots + k * SMP_USER_SIZE);
    uint8_t *d = (uint8_t *)h + SMP_USER_DATA;
    uint32_t n = 44100u, z, i;
    memset(h, 0, SMP_USER_SIZE);
    h->magic = SMP_USER_MAGIC;
    h->version = 1;
    h->nz = (uint8_t)nz;
    memcpy(h->name, name, strlen(name) < 8 ? strlen(name) : 8);
    for (z = 0; z < nz; z++) {
        uint32_t every = z ? n / 16u : n / 4u;
        for (i = 0; i < n; i++) {
            uint32_t t = i % every;
            x[i] = (int16_t)(t < 300u ? ((t & 8u) ? 20000 : -20000) * (int32_t)(300u - t) / 300 : 0);
        }
        ulp_ima(x, n, d + z * (n / 2u));
        h->zone[z].off = z * (n / 2u);
        h->zone[z].n = n;
        h->zone[z].le = n - 1u;
        h->zone[z].rate = 32768u;                       /* 22050 / 44100, Q16 */
        h->zone[z].root16 = 60 * 16;
        h->zone[z].hi = 127;
    }
    h->data_len = nz * (n / 2u);
    smp_user_scan(k);
}
/* the share (of 1000) of the bar's energy in the 1/32 after each of its first 'per' points (per a bar) */
static uint32_t ulp_onbeat(uint32_t bar, uint32_t per)
{
    int64_t on = 0, all = loop_energy(64, bar) + 1;
    uint32_t j;
    for (j = 0; j < per; j++)
        on += loop_energy(j * bar / per + (j ? 0u : 64u), j * bar / per + bar / 32u);
    return (uint32_t)(on * 1000 / all);
}
static void user_loop_test(void)
{
    static const int16_t U0[] = {P_E0, NLOOP, -1}, U1[] = {P_E0, NLOOP + 1, -1}, U9[] = {P_E0, NLOOP + 9, -1},
                         U0S[] = {P_E0, NLOOP, P_E2, 127, -1};
    uint32_t i, n = 0, bar = 88200u, on;
    char b[16];
    ulp_slot(0, "amen", 2);
    nl_build();
    for (i = 0; i < nl.n; i++)
        n += nl.s[i].kind == NLK_ULOOP && nl.s[i].cat == NC_BEAT;
    ok("user loops: a slot of two loops: named after the slot (AMEN 1, AMEN 2), in the library with NCC_LOOP",
       n == (NCC_LOOP ? 2u : 0u) && lu_name(0, b) && !strcmp(b, "AMEN 1") && lu_name(1, b) && !strcmp(b, "AMEN 2") &&
       !lu_name(2, b));
    loop_play(0, U0, 60, 0, bar + bar / 2, 120);
    printf("ncc: user loop: %u samples at %u Hz, ready %u\n", lu[1].len, lu[1].rate, lu[1].ready);
    ok("user loops: decoded in the main loop into R's buffer (a bar of 22.05 kHz: every 3rd sample, 7350 Hz)",
       lu[1].ready && lu[1].len == 14700u && lu[1].rate == 7350u);
    on = ulp_onbeat(bar, 4);
    printf("ncc: user loop AMEN 1: %u / 1000 of the energy right after the beats\n", on);
    ok("user loops: AMEN 1 plays its clicks on the Circuit's beats (in time, a note as the gate)",
       loop_energy(0, bar) > 0 && on > 800u);
    loop_play(0, U1, 37, 0, bar + bar / 2, 120);
    on = ulp_onbeat(bar, 16);
    ok("user loops: AMEN 2 (the slot's second zone) on every 16th, any note", loop_energy(0, bar) > 0 && on > 800u);
    loop_play(0, U0, 60, 0, bar * 2u, 90);
    ok("user loops: at 90 BPM its clicks still on the beats", ulp_onbeat(117600u, 4) > 800u);
    loop_play(0, U0S, 60, 0, bar + bar / 2, 120);
    ok("user loops: SHUF 127 moves its slices off the beats", ulp_onbeat(bar, 4) < 700u && loop_energy(0, bar) > 0);
    loop_play(0, U9, 60, 0, bar, 120);
    ok("user loops: a zone the slot does not have: silent", loop_energy(0, bar) == 0 && !lu[1].ready);
    ((smp_user_hdr_t *)host_slots)->crc ^= 1u;          /* uploaded again: decoded again */
    smp_user_scan(0);
    trk[1].p[P_E0] = (int16_t)NLOOP;
    loop_user_poll();
    n = lu[1].ready;
    for (i = 0; i < 24u; i++)
        loop_user_poll();
    ok("user loops: the slot uploaded again: decoded again (silent meanwhile)", !n && lu[1].ready);
    memset(host_slots, 0, sizeof host_slots);           /* (the other tests: empty slots) */
    smp_user_scan(0);
    loop_user_poll();
    ok("user loops: the slot emptied: silent, no loop in the library", !lu[1].ready && !lu_name(0, b));
}

/* the NCC commands: one request -> ed_out / ed_n */
static uint32_t ncc_req(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    ed_n = 0;
    return ed_ncc_handle(cmd, a, n) ? ed_n : 0u;
}
static void editor_ncc_test(void)
{
    uint8_t q[4];
    uint32_t total, got = 0, start, i, at, loopi = 0, found = 0;
    power();
    nl_build();
    ok("editor NCC: INFO says NCC, the types, the engines (LOOP only with NCC_LOOP)",
       ncc_req(ED_NCC_INFO, 0, 0) > 0 && !strcmp((const char *)ed_out, "NCC") && memmem(ed_out, ed_n, "PLUCK", 6) &&
       memmem(ed_out, ed_n, "ACID", 5) && !memmem(ed_out, ed_n, "LOOP", 5) == !NCC_LOOP);
    for (start = 0;; start += 16u) {                    /* the library, 16 at a time */
        q[0] = (uint8_t)((start + 8192u) & 127u);
        q[1] = (uint8_t)((start + 8192u) >> 7);
        q[2] = 16;
        ncc_req(ED_NCC_LIB, q, 3);
        total = (uint32_t)(ed_out[3] | ed_out[4] << 7) - 8192u;
        for (i = 0, at = 5; i < ed_out[2]; i++, got++) {
            if (ed_out[at + 1] == ENGI_ACID && !found)
                loopi = start + i, found = 1;
            at += 3;
            while (ed_out[at++])
                ;
        }
        if (start + 16u >= total)
            break;
    }
    ok("editor NCC: LIB pages through the whole library", total == nl.n && got == nl.n && found);
    q[0] = 1;
    q[1] = (uint8_t)((loopi + 8192u) & 127u);
    q[2] = (uint8_t)((loopi + 8192u) >> 7);
    ok("editor NCC: LOAD an ACID sound on R: R plays ACID, L untouched, the focus kept",
       ncc_req(ED_NCC_LOAD, q, 3) == 2u && ed_out[1] == 0 && trk[1].eng_req == ENGI_ACID && trk[0].eng_req == ENGI_FM6 &&
       song.sel == 0u);
    ncc_req(ED_NCC_PARTS, 0, 0);
    {
        uint32_t a0 = 1, e0, e1;
        e0 = (uint32_t)(ed_out[a0] | ed_out[a0 + 1] << 7) - 8192u;
        a0 += 6;
        while (ed_out[a0++])
            ;
        e1 = (uint32_t)(ed_out[a0] | ed_out[a0 + 1] << 7) - 8192u;
        ok("editor NCC: PARTS: R on that sound (its entry, ACID, channel 4, its name: ACID LEAD)", ed_out[0] == 2u &&
           e1 == loopi && ed_out[a0 + 5] == ENGI_ACID && ed_out[a0 + 4] == 3u &&
           !strcmp((const char *)ed_out + a0 + 6, "ACID LEAD") && e0 < nl.n);
    }
    q[0] = 1; q[1] = 0;
    ncc_req(ED_NCC_SET, q, 2);
    ok("editor NCC: SET STEREO", !NCC_SPLIT() && ed_out[1] == 0);
    q[1] = 1;
    ncc_req(ED_NCC_SET, q, 2);
    ok("editor NCC: SET SPLIT", NCC_SPLIT() && ed_out[1] == 0);
    q[0] = 0; q[1] = 0; q[2] = 9;
    ncc_req(ED_NCC_SET, q, 3);
    ok("editor NCC: SET L's channel to 10", mch_get(0) == 9u && ed_out[1] == 0);
    q[0] = 3; q[1] = 1; q[2] = NC_LEAD; q[3] = 5;
    ncc_req(ED_NCC_SET, q, 4);
    ok("editor NCC: SET R's filters (LEAD / ACID), its sound kept", lv.cat[1] == NC_LEAD && lv.ep[1] == 5u &&
       trk[1].eng_req == ENGI_ACID);
    q[0] = 0; q[1] = 5; q[2] = 0;
    ok("editor NCC: bad arguments answer rc 1", ncc_req(ED_NCC_SET, q, 3) == 2u && ed_out[1] == 1);
}

/* every NCC sound plays: a chord (POLY) or a note (LEGATO) on R, its peak and loudness; none silent, none clipping */
static void ncc_sounds_test(void)
{
    uint32_t k, b, i, okv = 1;
    for (k = 0; k < NCC_NSND; k++) {
        const ncc_snd_t *j = &NCC_SND[k];
        int32_t peak = 0;
        int64_t e = 0;
        power();
        fx_quiet();
        ncc_snd_load(&trk[1], k);
        lv_frame();
        for (b = 0; b < 4u; b++)
            blk();
        midi(0x93, j->mono ? 36 : 60, 100);
        if (!j->mono) {
            midi(0x93, 64, 100);
            midi(0x93, 67, 100);
        }
        for (b = 0; b < 700u; b++) {
            if (b == 450u) {
                midi(0x83, j->mono ? 36 : 60, 0);
                midi(0x83, 64, 0);
                midi(0x83, 67, 0);
            }
            blk();
            for (i = 0; i < 2u * CTL; i++) {
                int32_t x = blk_out[i] < 0 ? -blk_out[i] : blk_out[i];
                if (x > peak)
                    peak = x;
                e += (int64_t)(blk_out[i] >> 4) * (blk_out[i] >> 4);
            }
        }
        e /= 700 * 2 * CTL;
        printf("ncc: sound %-13s %-5s peak %6d  rms %5d\n", j->name, ENGINES[j->eng]->name, peak,
               (int32_t)(sqrt((double)e) * 16.0));
        if (peak < 1500 || peak >= 32000)
            okv = 0;
    }
    ok("NCC sounds: every one plays (a chord or a note), none silent, none clipping", okv);
}

/* a stub bank (FM6's voice bank, fm6_vbank.c, has no flash here): voice 0 "E.PIANO 1", 1 "STRINGS" */
static int stub_bank(uint32_t k, uint8_t *pk)
{
    static const char *const N[2] = {"E.PIANO 1 ", "STRINGS   "};
    memset(pk, 0, FM6_PACKED);
    if (k > 1u)
        return 1;
    memcpy(pk + 118, N[k], 10);
    return 0;
}
static const char *snd_name(uint32_t part)            /* the part's sound: NCC's own, or the engine's preset */
{
    const track_t *t = &trk[part];
    return lv.juno[part] ? NCC_SND[lv.juno[part] - 1u].name : ENGINES[eng_idx(t->eng_req)]->presets[t->preset].name;
}
/* a full bank (32 voices: fm6_bank_used 0xFFFFFFFF) after a power-on: its names read (before 0.3 the cache's key,
 * used + 1, wrapped to 0 and said "read") */
static int stub_bank32(uint32_t k, uint8_t *pk)
{
    memset(pk, 0, FM6_PACKED);
    memcpy(pk + 118, k & 1u ? "SYN-BASS 1" : "E.PIANO 1 ", 10);
    return 0;
}
static void full_bank_test(void)
{
    uint32_t i, named = 0, keys = 0;
    memset(&nl, 0, sizeof nl);                          /* (as at power-on) */
    fm6_bank_read = stub_bank32;
    fm6_bank_used = 0xFFFFFFFFu;
    nl_build();
    for (i = 0; i < nl.n; i++)
        if (nl.s[i].kind == NLK_BANK) {
            named += nl.bname[nl.s[i].k][0] != 0;
            keys += nl.s[i].cat == NC_KEYS;
        }
    ok("sounds: a full DX7 bank (32 voices) after a power-on: every voice named and typed", named == 32u && keys == 16u);
    fm6_bank_read = 0;
    fm6_bank_used = 0;
    nl.bok = 0;
}
static void engine_ui_test(void)
{
    track_t *t = &trk[0];
    uint32_t i, n[NC_N] = {0}, user = 0;
    power();
    nl_build();
    for (i = 0; i < nl.n; i++) {
        n[nl.s[i].cat]++;
        user += nl.s[i].kind == NLK_USER;
    }
    printf("ncc: library: %u sounds, BASS %u PAD %u LEAD %u KEYS %u PLUCK %u BEAT %u OTHER %u\n", nl.n, n[NC_BASS],
           n[NC_PAD], n[NC_LEAD], n[NC_KEYS], n[NC_PLUCK], n[NC_BEAT], n[NC_OTHER]);
    ok("sounds: the library holds the five engines' factory sounds and NCC's own, each in a category (none OTHER)",
       nl.n - user == 51u + NCC_NSND - 12u + (NCC_LOOP ? NLOOP : 0u) && n[NC_OTHER] == 0u && n[NC_BASS] == 18u &&
       n[NC_PAD] == 15u && n[NC_LEAD] == 20u && n[NC_KEYS] == 13u && n[NC_PLUCK] == 15u &&
       n[NC_BEAT] == (NCC_LOOP ? NLOOP : 0u));
    ok("sounds: LOOP is coming soon (core.h NCC_LOOP 0): not among the engines, no loop in the library",
       NCC_LOOP || (lv_eng_pos(ENGI_LOOP) < 0 && !nl_count(NC_BEAT, 0)));
    ok("sounds: names of other sounds: E.PIANO 1 KEYS, Strings 2 PAD, DEEP BASS BASS, POLY SYNTH KEYS, WEIRD OTHER",
       nl_cat("E.PIANO 1", ENGI_FM6) == NC_KEYS && nl_cat("Strings 2", ENGI_FM6) == NC_PAD &&
       nl_cat("DEEP BASS", ENGI_ACID) == NC_BASS && nl_cat("POLY SYNTH", ENGI_ANALOG) == NC_KEYS &&
       nl_cat("WEIRD", ENGI_TRIO) == NC_OTHER);
    ok("sounds: a long keyword inside a word: GrandPiano and Glidepiano KEYS, SynthOrgan KEYS, (but no EP in DEEP)",
       nl_cat("GrandPiano", ENGI_FM6) == NC_KEYS && nl_cat("Glidepiano", ENGI_FM6) == NC_KEYS &&
       nl_cat("SynthOrgan", ENGI_FM6) == NC_KEYS && nl_cat("DEEP", ENGI_FM6) == NC_OTHER);
    {                                                   /* the DX7 classics by their names: 8 KEYS, 6 of the rest each */
        static const char *const DX[32] = {"E.PIANO 1", "E.PIANO 2", "E.PIANO 3", "PIANO   1", "SYN-PIANO", "CLAV    1",
            "FUNK CLAV", "E.ORGAN 1", "BASS    1", "BASS    2", "SYN-BASS 1", "SYN-BASS 2", "E.BASS  1", "PLUCK BASS",
            "STRINGS 1", "STRG ENS 1", "HIGH STRGS", "SYN-VOX", "CHIME-STRG", "VOICES", "SYN-LEAD 1", "BRASS   1",
            "SYNBRASS 1", "LEAD BRASS", "HARMONICA1", "FLUTE   1", "TUB BELLS", "MARIMBA", "VIBE    1", "HARP    1", "KOTO",
            "GLOKENSPL"};
        static const uint8_t WANT[32] = {NC_KEYS, NC_KEYS, NC_KEYS, NC_KEYS, NC_KEYS, NC_KEYS, NC_KEYS, NC_KEYS,
            NC_BASS, NC_BASS, NC_BASS, NC_BASS, NC_BASS, NC_BASS, NC_PAD, NC_PAD, NC_PAD, NC_PAD, NC_PAD, NC_PAD,
            NC_LEAD, NC_LEAD, NC_LEAD, NC_LEAD, NC_LEAD, NC_LEAD, NC_PLUCK, NC_PLUCK, NC_PLUCK, NC_PLUCK, NC_PLUCK, NC_PLUCK};
        uint32_t good = 1;
        for (i = 0; i < 32u; i++)
            if (nl_cat(DX[i], ENGI_FM6) != WANT[i]) {
                printf("  %s: %s\n", DX[i], NC_NAME[nl_cat(DX[i], ENGI_FM6)]);
                good = 0;
            }
        ok("sounds: 32 DX7 classics (E.PIANO 1 .. GLOKENSPL) each in the type it is", good);
    }
    t_turn(EN_SELECT, 1);
    ok("sounds: SELECT on SOUND: nothing yet while turning, the pad shows it coming", lv.cpend == 1 && lv.cat[0] == NC_ALL);
    lv_frames(400);
    ok("sounds: after the rest: L BASS, its first bass loaded (FM BASS), R untouched, the popup",
       lv.cat[0] == NC_BASS && t->eng_req == ENGI_FM6 && !strcmp(t->eng_req == ENGI_FM6 ?
       ENGINES[ENGI_FM6]->presets[t->preset].name : "", "FM BASS") && trk[1].eng_req == ENGI_ANALOG && lv.say &&
       !strcmp(lv.say, "L BASS / ALL"));
    t_turn(EN_ALGO, -1);
    lv_frames(400);
    ok("sounds: ALGORITHM back from ALL: nothing (not round: ALL is the first)", lv.ep[0] == 0u &&
       !strcmp(snd_name(0), "FM BASS"));
    t_turn(EN_ALGO, 5);
    lv_frames(400);
    ok("sounds: ALGORITHM on to ACID, its first bass (NCC's SOFT BASS)", lv.ep[0] == 5u &&
       t->eng_req == ENGI_ACID && !strcmp(snd_name(0), "SOFT BASS"));
    ok("sounds: a new ACID sound slides (LEGATO, GLIDE in TIME mode)", t->p[P_VOICE] == V_LEGATO &&
       t->p[P_GLMODE] == 1 && t->p[P_GLIDE] == LV_ACID_GLIDE);
    t_turn(EN_PRESET, 1);
    lv_frames(200);
    ok("sounds: PRESETS: the next ACID bass (ACID 303, the engine's own)", t->eng_req == ENGI_ACID &&
       !strcmp(snd_name(0), "ACID 303"));
    t_turn(EN_SELECT, 1);
    lv_frames(400);
    ok("sounds: SELECT on: ACID has no PAD, so LEAD (ACID LEAD)", lv.cat[0] == NC_LEAD && t->eng_req == ENGI_ACID &&
       !strcmp(snd_name(0), "ACID LEAD"));
    t_turn(EN_ALGO, 1);
    lv_frames(400);
    ok("sounds: ALGORITHM on past the last engine: it stays (not round)", lv.ep[0] == 5u);
    t_turn(EN_ALGO, -5);
    lv_frames(400);
    ok("sounds: ALGORITHM back to ALL engines, the lead kept", lv.ep[0] == 0u && !strcmp(snd_name(0), "ACID LEAD"));
    t_turn(EN_PRESET, 3);
    lv_frames(200);
    ok("sounds: PRESETS over the engines: SQUELCH LEAD, SCREAM, HOOVERISH", t->eng_req == ENGI_ACID &&
       !strcmp(snd_name(0), "HOOVERISH"));
    t_turn(EN_PRESET, 1);
    lv_frames(200);
    ok("sounds: PRESETS past the last lead: it stays (not round)", !strcmp(snd_name(0), "HOOVERISH"));
    t_turn(EN_PRESET, -60);
    lv_frames(200);
    ok("sounds: PRESETS far back: the first lead, FM6's (BRASS), and no further", t->eng_req == ENGI_FM6 &&
       !strcmp(snd_name(0), "BRASS"));
    fm6_bank_read = stub_bank;                          /* a bank of two voices: E.PIANO 1 among the KEYS */
    fm6_bank_used = 3u;
    lv.cat[0] = NC_KEYS;
    lv.ep[0] = 1u;
    t_turn(EN_PRESET, 3);                               /* TINE EP, ORGAN, E.PIANO 1 */
    lv_frames(200);
    ok("sounds: a DX7 bank voice in the list by its name (E.PIANO 1: KEYS), loaded as FM6's slot B1",
       t->eng_req == ENGI_FM6 && t->p[P_E7] == (int16_t)(FM6_OWN + 1u) && !strcmp(nl.bname[0], "E.PIANO 1"));
    fm6_bank_read = 0;
    fm6_bank_used = 0;
    nl.bok = 0;
    lv_press(B_OCTUP);
    t_turn(EN_ALGO, 4);
    lv_frames(400);
    ok("sounds: R's filters its own: TRIO, its sound", lv.ep[1] == 4u && trk[1].eng_req == ENGI_TRIO && lv.ep[0] == 1u);
    (void)project_save(0);
    t->eng_req = ENGI_ACID;
    project_load(0);
    for (i = 0; i < 3u; i++)
        lv_frame();
    ok("sounds: the parts' engines come back with the project", trk[0].eng_req == ENGI_FM6 && trk[1].eng_req == ENGI_TRIO);
    lv_press(B_ARP);
    t_turn(EN_SELECT, 1);
    ok("sounds: SELECT on ARP is its page 2, not the filter", lv.sub == 1u && !lv.cpend);
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
        midi(0xB3, 71, 127);                            /* FILTER FREQ */
        for (i = 0; i < 30u; i++)
            blk();
        okv &= trk[1].p[WANT[k].id] > base;
    }
    ok("engines: FILTER FREQ moves PHASE's DCW, TRIO's CUT, ACID's CUT", okv);
}

static void macro_map_test(void)
{
    uint32_t e, m, i, moved = 1, fits = 1, named = 1;
    char r[24], l[24], cut[24];
    int32_t pit;
    for (e = 0; e < NELEM(NCM_MAP); e++)
        for (m = 0; m < NCM_N; m++) {
            const ncm_map_t *mp = &NCM_MAP[e];
            uint32_t id = mp->d[m][0].id, cc;
            int32_t base, now;
            power();
            set_engine_of(&trk[1], mp->eng);
            for (i = 0; i < 5u; i++)
                blk();
            base = ncm_get(&trk[1], id);
            for (cc = 0; cc < 128u; cc += 127u) {           /* (one way may be the end of its range) */
                midi(0xB3, NCM_CC[m], cc);
                for (i = 0; i < 30u; i++)
                    blk();
                if (ncm_get(&trk[1], id) != base)
                    break;
            }
            now = ncm_get(&trk[1], id);
            if (now == base) {
                printf("  %s knob %u: parameter %u stays %d\n", ENGINES[mp->eng]->name, m + 1u, id, base);
                moved = 0;
            }
            lv_macro_text(1, m, r);
            l[0] = 'R';
            l[1] = ' ';
            str_cpy(l + 2, NCM_NAME[m], 20);
            if (6 + text_w(&AF_M, l) + 8 > 234 - text_w(&AF_S, r)) {
                printf("  %s knob %u: \"%s\" + \"%s\" too wide\n", ENGINES[mp->eng]->name, m + 1u, l, r);
                fits = 0;
            }
            if (mp->show[m] && strncmp(r, mp->show[m], strlen(mp->show[m])))
                named = 0;
            if (mp->eng == ENGI_ACID && m == 4u)
                strcpy(cut, r);
        }
    ok("macros: every engine's 8 knobs move their parameter", moved);
    ok("macros: the popup's text fits beside the legend", fits);
    ok("macros: the popup names what the knob does", named);
    ok("macros: ACID's FILTER FREQ reads its cutoff", !strncmp(cut, "CUTOFF ", 7) && strstr(cut, "Hz"));
    power();                                            /* ANALOG WAVE SUB: OSCILLATOR is the sub's level */
    set_engine_of(&trk[1], ENGI_ANALOG);
    trk[1].p[P_E0] = 6;
    midi(0xB3, 1, 100);
    midi(0xB3, 2, 100);
    for (i = 0; i < 30u; i++)
        blk();
    lv_macro_text(1, 0, r);
    lv_macro_text(1, 1, l);
    ok("macros: ANALOG SUB: OSCILLATOR says SUB, OSC MOD DTN", !strncmp(r, "SUB ", 4) && !strncmp(l, "DTN ", 4));
    power();                                            /* FM6's MODULATION: under a semitone of vibrato */
    pit = trk[0].p[P_LD_PIT];
    midi(0xB2, 12, 127);
    for (i = 0; i < 30u; i++)
        blk();
    lv_macro_text(0, 2, r);
    ok("macros: FM6's MODULATION: a little vibrato; AMP ENV reads CAR ENV",
       trk[0].p[P_LD_PIT] > pit && (trk[0].p[P_LD_PIT] - pit) * 3 <= 16 && !strncmp(r, "CAR ENV ", 8));
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
/* the bar on the external clock: 16 bars of pulses whose USB times jitter (the tempo's measurement wobbles), the
 * Circuit's every bar start (pulse 0, 96, 192 ..) is NCC's bar start too: ncc_bar near 0, no drift */
static void bar_lock_test(void)
{
    uint32_t k, b, worst = 0, bar, rnd = 12345u;
    power();
    song.g[G_CLOCK] = 1;
    blk();
    ext_pulses(8);
    midi(0xFA, 0, 0);                                   /* Start */
    for (k = 0; k < 16u * 96u; k++) {
        midi(0xF8, 0, 0);
        for (b = 0; b < 29u; b++) {
            if (b % 4u == 0u) {
                rnd = rnd * 1103515245u + 12345u;
                fm1_ms += 2u + (rnd >> 16) % 3u;        /* 2 .. 4 ms: USB's jitter on the pulses' times */
            }
            blk();
            if (b == 0u && k % 96u == 0u && k) {
                uint32_t d;
                bar = 4u * beat_samples();
                d = ncc_bar % bar;
                d = d < bar / 2u ? d : bar - d;
                worst = d > worst ? d : worst;
            }
        }
    }
    printf("ncc: bar lock: the worst bar start %u samples off over 16 bars (a 1/16 is %u)\n", worst, 4u * beat_samples() / 16u);
    ok("bar lock: on the external clock every bar of the Circuit starts NCC's bar (no drift over 16 bars)",
       worst < 4u * beat_samples() / 64u);
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

#if NCC_LOOP
/* the editor's NCC commands on a user slot's loops: LIB names them after the slot, PARTS names the one on R */
static void editor_ncc_uloop_test(void)
{
    uint8_t q[3];
    uint32_t start, i, at, total, found = 0, idx = 0;
    power();
    ulp_slot(0, "amen", 2);
    for (start = 0;; start += 16u) {
        q[0] = (uint8_t)((start + 8192u) & 127u);
        q[1] = (uint8_t)((start + 8192u) >> 7);
        q[2] = 16;
        ncc_req(ED_NCC_LIB, q, 3);
        total = (uint32_t)(ed_out[3] | ed_out[4] << 7) - 8192u;
        for (i = 0, at = 5; i < ed_out[2]; i++) {
            if (ed_out[at] == NLK_ULOOP && !strcmp((const char *)ed_out + at + 3, "AMEN 1"))
                found = 1, idx = start + i;
            at += 3;
            while (ed_out[at++])
                ;
        }
        if (start + 16u >= total)
            break;
    }
    ok("editor NCC: LIB names a user slot's loops after the slot (AMEN 1)", found);
    q[0] = 1;
    q[1] = (uint8_t)((idx + 8192u) & 127u);
    q[2] = (uint8_t)((idx + 8192u) >> 7);
    ncc_req(ED_NCC_LOAD, q, 3);
    ncc_req(ED_NCC_PARTS, 0, 0);
    at = 7;
    while (ed_out[at++])
        ;
    ok("editor NCC: PARTS names the user loop R plays (AMEN 1)", !strcmp((const char *)ed_out + at + 6, "AMEN 1"));
}
#endif

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
    full_bank_test();
    ncc_sounds_test();
    loop_test();
    editor_ncc_test();
    user_loop_test();
#if NCC_LOOP
    editor_ncc_uloop_test();
#endif
    engine_macro_test();
    macro_map_test();
    acid_test();
    stop_test();
    bar_lock_test();
    splash_test();
    bpm0_test();
    printf("ncc: %s\n", bad ? "FAILED" : "all ok");
    return bad ? 1 : 0;
}
