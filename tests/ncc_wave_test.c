/* SPDX-License-Identifier: GPL-3.0-only */
/* Host checks for NCC's WAVE engine (eng_wave.c) and NCC's own FM6 sounds (ncc_fm6_presets.c), included by
 * ncc_test.c after ncc_extra_engines.c (extra_audio: a forked render, the same state for every comparison). */

/* the absolute sample sum of 200 blocks of a held note on R with WAVE's TABLE tb, POS pos, ENV env */
static uint32_t wave_sum(uint32_t tb, int32_t pos, int32_t env, uint32_t note, uint64_t *hash)
{
    track_t *t = &trk[1];
    uint32_t b, i, sum = 0;
    power();
    fx_quiet();
    set_engine_of(t, ENGI_WAVE);
    apply_preset_to(t, 0);
    t->p[P_E0] = (int16_t)tb;
    t->p[P_E1] = (int16_t)pos;
    t->p[P_E2] = (int16_t)env;
    t->p[P_E3] = 0;
    t->p[P_E4] = 127;
    t->p[P_E5] = 0;
    t->p[P_ED_FX] = 0;
    for (b = 0; b < 40u; b++)
        blk();
    *hash = 1469598103934665603ull;
    midi(0x93, note, 100);
    for (b = 0; b < 200u; b++) {
        blk();
        for (i = 0; i < 2u * CTL; i++) {
            sum += (uint32_t)(blk_out[i] < 0 ? -blk_out[i] : blk_out[i]) >> 6;
            *hash = (*hash ^ (uint32_t)blk_out[i]) * 1099511628211ull;
        }
    }
    midi(0x83, note, 0);
    return sum;
}

static void wave_test(void)
{
    static const uint8_t HARM[NW_LEVELS] = {64, 24, 9, 3};
    uint32_t tb, k, lv, i, m, good = 1, n;
    uint64_t h0, h1, h2;
    for (tb = 0; tb < NW_TABLES; tb++)                  /* the data: each row closes on its first sample, the */
        for (k = 0; k < NW_WAVES; k++) {                /* fullest level of a wave reaches +-127, no level silent */
            int32_t top = 0;
            for (lv = 0; lv < NW_LEVELS; lv++) {
                int32_t pk = 0;
                for (i = 0; i < NW_LEN; i++) {
                    int32_t x = NCC_WAVES[tb][k][lv][i];
                    pk = (x < 0 ? -x : x) > pk ? (x < 0 ? -x : x) : pk;
                }
                good &= NCC_WAVES[tb][k][lv][NW_LEN] == NCC_WAVES[tb][k][lv][0] && pk > 20;
                top = pk > top ? pk : top;
            }
            good &= top == 127;
        }
    ok("WAVE: 5 tables x 8 waves x 4 levels: closed cycles, full scale, none silent", good);
    good = 1;
    for (n = 12; n <= 108u; n++) {                      /* the level a note reads keeps its top harmonic below FS / 2 */
        uint32_t inc = pitch_inc(n * 16u);
        uint32_t hz16 = (uint32_t)(((uint64_t)inc * FS) >> 28);   /* the note's frequency, 1/16 Hz */
        lv = wave_level(inc);
        good &= HARM[lv] * hz16 < 16u * (uint32_t)FS / 2u && (!lv || HARM[lv - 1u] * hz16 >= 16u * 21000u);
    }
    ok("WAVE: C0 .. C8: the level read stays below half the sample rate, and is the fullest that does", good);
    good = 1;
    for (tb = 0; tb < NW_TABLES; tb++) {
        uint32_t a = wave_sum(tb, 0, 0, 60, &h0), b = wave_sum(tb, 127, 0, 60, &h1), c = wave_sum(tb, 64, 0, 60, &h2);
        if (!(a > 2000u && b > 2000u && c > 2000u && h0 != h1 && h1 != h2 && h0 != h2)) {
            printf("    WAVE table %u: sums %u %u %u\n", tb, a, c, b);
            good = 0;
        }
    }
    ok("WAVE: every table sounds at POS 0, 64 and 127, each another wave", good);
    (void)wave_sum(2, 20, 0, 60, &h0);
    (void)wave_sum(2, 20, 63, 60, &h1);
    (void)wave_sum(2, 20, -64, 60, &h2);
    ok("WAVE: ENV moves the position with the envelope, either way", h0 != h1 && h0 != h2 && h1 != h2);
    {
        uint32_t lo = wave_sum(4, 127, 0, 24, &h0), hi = wave_sum(4, 127, 0, 108, &h1);
        ok("WAVE: the lowest and a very high note both play, bounded", lo > 2000u && hi > 500u);
    }
    {
        extra_audio_t reference = extra_audio(ENGI_WAVE, 0, 0, 64), repeat = extra_audio(ENGI_WAVE, 0, 0, 64);
        good = reference.hash == repeat.hash && reference.peak > 500;
        for (m = 0; m < NCM_N; m++) {
            extra_audio_t low = extra_audio(ENGI_WAVE, 0, m, 0), high = extra_audio(ENGI_WAVE, 0, m, 127);
            if ((low.hash == reference.hash && high.hash == reference.hash) || !low.peak || !high.peak ||
                low.peak > 32768u || high.peak > 32768u) {
                printf("    WAVE knob %u has no audio effect or unhealthy output (%u/%u)\n", m + 1u, low.peak, high.peak);
                good = 0;
            }
        }
        ok("WAVE: all 8 NC controls change rendered audio with bounded output", good);
    }
    power();
    nl_build();
    for (i = 0, n = 0; i < nl.n; i++)
        n += nl.s[i].eng == ENGI_WAVE;
    ok("WAVE: in NCC's engine list (last) with its sounds in the library", lv_eng_pos(ENGI_WAVE) ==
       (int32_t)NELEM(LV_ENGS) - 1 && n == NELEM(WAVE_PRESETS) + ncc_count(ENGI_WAVE) && n >= 10u);
}

/* NCC's FM6 sounds: the patch goes in as the part's own (SLOT OWN), named as the sound; FM6's factory eight stay
 * first in the library; UNDO brings the patch from before back */
static void ncc_fm6_sounds_test(void)
{
    track_t *t = &trk[0];
    uint8_t v[FP_SIZE + 1u], before[FP_SIZE + 1u];
    uint32_t k, i, first = 0, good = 1, seen = 0;
    power();
    memcpy(before, fm6_patch[0], sizeof before);
    for (k = 0; k < NCC_NSND; k++) {
        const ncc_snd_t *j = &NCC_SND[k];
        if (j->eng != ENGI_FM6)
            continue;
        seen++;
        ncc_snd_load(t, k);
        lv_frame();
        fm6_unpack(NCC_FM6[(uint32_t)j->e[7]], v);
        for (i = 0; i < 10u; i++)
            v[FP_NAME + i] = (uint8_t)(i < strlen(j->name) ? j->name[i] : ' ');
        fm6_sanitize(v);
        good &= t->eng_req == ENGI_FM6 && t->p[P_E7] == (int16_t)FM6_OWN && !memcmp(v, fm6_patch[0], FP_SIZE) &&
                (uint32_t)j->e[7] < NCC_NFM6;
        for (i = 0; i < 6u; i++)                        /* (no amplitude modulation sensitivity: ncc_fm6_presets.c) */
            good &= !(v[i * FP_OP + FP_AMS] & 3u);
    }
    ok("NCC FM6 sounds: each loads its own patch as the part's (SLOT OWN), named as the sound", good && seen ==
       NCC_NFM6);
    nl_build();
    for (i = 0; i < nl.n && nl.s[i].eng == ENGI_FM6 && nl.s[i].kind == NLK_PRESET; i++)
        first++;
    ok("NCC FM6 sounds: after FM6's factory eight in the library", first == ENGINES[ENGI_FM6]->npresets &&
       nl.s[first].kind == NLK_NCC && nl.s[first].eng == ENGI_FM6);
}
