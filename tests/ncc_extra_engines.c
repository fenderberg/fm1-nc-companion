/* SPDX-License-Identifier: GPL-3.0-only */
/* Host behavior/audio checks for the extra engines, included by ncc_test.c. Forks give each comparison the
 * identical DSP/random/clock state; neither differing RNG seeds nor lingering effect tails can pass a knob. */
#include <unistd.h>
#include <sys/wait.h>

static const uint8_t EXTRA_ENG[] = {9, 8, 5, 3, 7, 11, 4};
typedef struct { uint64_t hash; uint32_t peak; } extra_audio_t;
static extra_audio_t extra_audio(uint32_t eng, uint32_t preset, uint32_t macro, uint32_t value)
{
    int fd[2], status;
    pid_t pid;
    extra_audio_t result = {0, 0};
    fflush(stdout);
    if (pipe(fd)) { bad++; return result; }
    pid = fork();
    if (!pid) {
        uint32_t b, i;
        track_t *t = &trk[1];
        close(fd[0]);
        power(); fx_quiet();
        set_engine_of(t, eng); apply_preset_to(t, preset);
        t->p[P_ATK] = 12; t->p[P_DEC] = 64; t->p[P_SUS] = 90; t->p[P_REL] = 60;
        t->p[P_ED_FLT] = 0;
        t->p[P_VOICE] = V_POLY;
        t->p[P_DIST] = t->p[P_CHOR] = 0;
        /* Leave room for both directions; a fully open tone filter masks positive modulation. */
        if (eng == 3u) t->p[P_E7] = 60;
        if (eng == 4u) t->p[P_E4] = 72;
        for (b = 0; b < 140u; b++) blk();
        result.hash = 1469598103934665603ull;
        midi(0x93, 60, 100);
        for (b = 0; b < 900u; b++) {
            if (b == 100u) midi(0xB3, NCM_CC[macro], value); /* live CC on a held note */
            if (b == 300u) midi(0x93, 67, 100);             /* attack/percussion/grain parameters too */
            if (b == 500u) { midi(0x83, 60, 0); midi(0x83, 67, 0); }
            blk();
            for (i = 0; i < 2u * CTL; i++) {
                uint32_t a = (uint32_t)(blk_out[i] < 0 ? -blk_out[i] : blk_out[i]);
                if (a > result.peak) result.peak = a;
                result.hash = (result.hash ^ (uint32_t)blk_out[i]) * 1099511628211ull;
            }
        }
        if (write(fd[1], &result, sizeof result) != sizeof result) _exit(2);
        close(fd[1]); _exit(0);
    }
    close(fd[1]);
    if (pid < 0) { close(fd[0]); bad++; return result; }
    if (read(fd[0], &result, sizeof result) != sizeof result) bad++;
    close(fd[0]);
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) bad++;
    return result;
}
/* All pairs under note/CC pressure through the real Dual router, including PHYS/GRAIN on both sides. */
static void extra_dual_stress(void)
{
    uint32_t l, r, b, p, k, healthy = 1, sounding = 1, capped = 1;
    for (l = 0; l < NELEM(EXTRA_ENG); l++)
        for (r = 0; r < NELEM(EXTRA_ENG); r++) {
            uint32_t heard[2] = {0, 0};
            power(); fx_quiet(); song.g[G_ENGGO] = 0;
            set_engine_of(&trk[0], EXTRA_ENG[l]); set_engine_of(&trk[1], EXTRA_ENG[r]);
            ncc_set_dual(1);
            for (p = 0; p < 2u; p++) {
                trk[p].p[P_VOICE] = V_POLY;
                trk[p].p[P_ATK] = 0; trk[p].p[P_SUS] = 112;
                trk[p].p[P_CHOR] = trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
            }
            perf_blocks(40);
            for (k = 0; k < 3u; k++) midi(0x92, 60u + 4u * k, 100);
            for (b = 0; b < 800u; b++) {
                if (b % 40u == 20u) {
                    uint32_t m = (b / 40u) % NCM_N;
                    midi(0xB2, NCM_CC[m], b & 64u ? 127 : 0);
                    midi(0xB3, NCM_CC[m], b & 64u ? 0 : 127);
                }
                blk();
                for (p = 0; p < 2u; p++) {
                    uint32_t voices = 0;
                    for (k = 0; k < NVOICE; k++) voices += trk[p].v[k].active;
                    if (trk[p].engine == 9u || trk[p].engine == 8u) capped &= voices <= 3u;
                    for (k = 0; k < CTL; k++) {
                        int32_t sample = blk_out[2u * k + p];
                        healthy &= sample >= -32768 && sample <= 32767;
                        heard[p] += sample != 0;
                    }
                }
            }
            sounding &= heard[0] > 100u && heard[1] > 100u;
            midi(0xB2, 120, 0); midi(0xB3, 120, 0); perf_blocks(4);
            healthy &= !gate_on(&trk[0]) && !gate_on(&trk[1]);
        }
    ok("extra engines: all 49 Dual pairs render under live CCs and stop on panic", healthy && sounding);
    ok("extra engines: PHYS/GRAIN keep their three-voice cap under Dual", capped);
}
/* NOISE's pole histories can exceed 16 bits. Cover every source and both sides of the color crossfade
 * at high resonance/gain, including MIDI tweaks while the histories are nonzero (fatal UBSan is enabled). */
static void extra_noise_stress(void)
{
    uint32_t mode, color, b, k, healthy = 1;
    static const uint8_t colors[] = {1, 56, 64, 92, 127};
    for (mode = 0; mode < 4u; mode++)
        for (color = 0; color < NELEM(colors); color++) {
            power(); fx_quiet();
            set_engine_of(&trk[1], 11);
            trk[1].p[P_E0] = (int16_t)mode; trk[1].p[P_E1] = colors[color];
            trk[1].p[P_E2] = 60; trk[1].p[P_E3] = 127;
            trk[1].p[P_E5] = 127; trk[1].p[P_E6] = 127;
            trk[1].p[P_ATK] = 0; trk[1].p[P_SUS] = 127;
            perf_blocks(30);
            midi(0x93, 48, 127); midi(0x93, 60, 127); midi(0x93, 72, 127);
            for (b = 0; b < 1400u; b++) {
                if (b % 120u == 0u) midi(0xB3, 71, b & 128u ? 0 : 127);
                blk();
                for (k = 0; k < 2u * CTL; k++) healthy &= blk_out[k] >= -32768 && blk_out[k] <= 32767;
            }
        }
    ok("NOISE: every source/color boundary stays bounded under resonance, dense impulses and live cutoff", healthy);
}
static void extra_engines_test(void)
{
    uint32_t e, m, p, i, good = 1, isolated = 1, accurate = 1;
    uint32_t seed = 12345;
    for (i = 0; i < 100000u; i++) {
        uint32_t d, b, n;
        seed = seed * 1664525u + 1013904223u;
        d = 1u + seed % 749698u;
        seed = seed * 1664525u + 1013904223u;
        b = seed % (d < 65536u ? d : 65536u);
        seed = seed * 1664525u + 1013904223u;
        n = d + 1u + seed % (749699u - d);
        accurate &= ncc_formant_bw(b, n, d) == (uint32_t)((uint64_t)b * n / d);
    }
    ok("VOICE: narrow-division bandwidth equals the original wide quotient (100000 cases)",
       accurate && !ncc_formant_bw(10, 100, 0));
    for (e = 0; e < NELEM(EXTRA_ENG); e++) {
        extra_audio_t reference = extra_audio(EXTRA_ENG[e], 0, 0, 64);
        extra_audio_t repeat = extra_audio(EXTRA_ENG[e], 0, 0, 64);
        uint32_t audible = reference.hash == repeat.hash && reference.peak > 500;
        for (m = 0; m < NCM_N; m++) {
            extra_audio_t low = extra_audio(EXTRA_ENG[e], 0, m, 0);
            extra_audio_t high = extra_audio(EXTRA_ENG[e], 0, m, 127);
            if ((low.hash == reference.hash && high.hash == reference.hash) ||
                !low.peak || !high.peak || low.peak > 32768u || high.peak > 32768u) {
                printf("ncc: extra %s knob %u has no audio effect or unhealthy output (%u/%u)\n",
                       ENGINES[EXTRA_ENG[e]]->name, m + 1u, low.peak, high.peak);
                audible = 0;
            }
        }
        printf("ncc: extra %s: eight audible macros, deterministic reference: %s\n",
               ENGINES[EXTRA_ENG[e]]->name, audible ? "ok" : "FAIL");
        good &= audible;
        power();
        for (p = 0; p < 2u; p++) set_engine_of(&trk[p], EXTRA_ENG[e]);
        ncc_set_dual(2);                                /* CH4 notes to both; CC3 must still edit L only */
        for (i = 0; i < 40u; i++) blk();
        for (p = 0; p < 2u; p++)
            for (m = 0; m < NCM_N; m++) {
                int16_t untouched[P_COUNT];
                memcpy(untouched, trk[1u - p].p, sizeof untouched);
                midi(0xB2 + p, NCM_CC[m], 0);
                for (i = 0; i < 40u; i++) blk();
                isolated &= !memcmp(untouched, trk[1u - p].p, sizeof untouched);
                isolated &= ncm.tgt[p][m] == 0;
            }
    }
    ok("extra engines: all 56 NC controls change rendered audio with bounded output", good);
    ok("extra engines: all CCs stay on their own L/R part in Dual CH4", isolated);
    /* LOFI's triangle/saw/noise use sweep, STEP uses release; labels follow the actual destination. */
    power(); set_engine_of(&trk[1], 3);
    for (i = 0; i < 20u; i++) blk();
    midi(0xB3, 2, 0);
    for (i = 0; i < 40u; i++) blk();
    trk[1].p[P_E1] = 2; trk[1].p[P_E4] = trk[1].p[P_E2];
    /* Same numerical value as the old field: changing destinations must still re-anchor. */
    blk();
    {
        char s[24];
        int32_t sweep = trk[1].p[P_E4];
        midi(0xB3, 2, 127);
        for (i = 0; i < 40u; i++) blk();
        lv_macro_text(1, 1, s);
        ok("LOFI: waveform change preserves macro slew and routes OSC MOD to audible SWP",
           trk[1].p[P_E4] > sweep && !strncmp(s, "SWP ", 4) && ncm.tgt[1][1] == 127);
        trk[1].p[P_E0] = 4;
        lv_macro_text(1, 2, s);
        ok("LOFI: STEP AMP ENV is RELEASE (the chip has its own decay)",
           ncm_destination(&trk[1], ncm_map(&trk[1]), 2, 0).id == P_REL && !strncmp(s, "RELEASE ", 8));
    }
    for (p = 1; p <= 4u; p++) {                         /* all non-pulse/STEP factory modes */
        extra_audio_t a = extra_audio(3, p, 1, 64), b = extra_audio(3, p, 1, 127), c = extra_audio(3, p, 1, 0);
        ok("LOFI: OSC MOD changes audio in each factory waveform/chip mode", a.hash != b.hash || a.hash != c.hash);
    }
    extra_dual_stress();
    extra_noise_stress();
    {
        up_bank_t saved[UP_SLOTS / UP_PER_BANK];
        uint32_t factory = NCC_NSND, users = 0, banks = 0;
        memcpy(saved, up_bank, sizeof saved);
        power();
        for (i = 0; i < UP_SLOTS; i++) {
            up_rec_t *r = up_rec(i);
            memset(r, 0, sizeof *r);
            r->used = UP_USED; r->ver = UP_VER; r->engine = EXTRA_ENG[i % NELEM(EXTRA_ENG)];
            r->np = P_COUNT; memcpy(r->name, "PAD", 3);
        }
        fm6_bank_read = stub_bank32; fm6_bank_used = 0xFFFFFFFFu; nl.bok = 0;
        nl_build();
        for (e = 0; e < NELEM(LV_ENGS); e++)
            for (i = 0; i < ENGINES[LV_ENGS[e]]->npresets; i++)
                factory += preset_orig(ENGINES[LV_ENGS[e]], i) == i;
        for (i = 0; i < nl.n; i++) { users += nl.s[i].kind == NLK_USER; banks += nl.s[i].kind == NLK_BANK; }
        ok("library: full DX7 bank and all user slots fit with every factory sound; reserve also fits all loops",
           users == UP_SLOTS && banks == FM6_NBANK && nl.n >= factory + users + banks &&
           factory + UP_SLOTS + FM6_NBANK + LU_N <= NL_MAX);
        memcpy(up_bank, saved, sizeof saved);
        fm6_bank_read = 0; fm6_bank_used = 0; nl.bok = 0;
    }
}
