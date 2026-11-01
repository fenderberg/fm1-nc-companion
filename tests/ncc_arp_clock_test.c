/* SPDX-License-Identifier: GPL-3.0-only */
/* Test actual arp steps, rather than only the screen's bar counter. */
static void arp_clock_test(void)
{
    uint32_t source, k, b, elapsed, base, steps, missed, stray, previous, blocks;
    for (source = 1; source <= 2; source++) {
        power();
        song.g[G_CLOCK] = (int16_t)source;
        blk();
        trk[1].p[P_AMODE] = 1;
        trk[1].p[P_ARATE] = 2;                 /* 1/16: one step per six MIDI clocks */
        base = fm1_ms;
        elapsed = steps = missed = stray = 0;
        midi_enqueue(0xFA0Fu, source);
        midi(0x93, 60, 100);
        for (k = 0; k < 16u * 96u; k++) {
            /* Audio time and timestamps advance together. Alternate tempo each bar;
             * small deterministic timing variation models input clock jitter. */
            blocks = ((k / 96u) & 1u ? 24u : 34u) + k % 3u;
            midi_enqueue(0xF80Fu, source);
            for (b = 0; b < blocks; b++) {
                fm1_ms = base + elapsed * 1000u / (uint32_t)FS;
                previous = trk[1].arp_idx;
                if (!k && !b)
                    previous = 0xFFFFFFFFu;    /* the queued first note initializes the arp's index */
                blk();
                if (trk[1].arp_idx != previous) {
                    steps++;
                    stray += k % 6u != 0u || b != 0u;
                }
                if (!b && !(k % 6u))
                    missed += trk[1].arp_idx == previous;
                elapsed += CTL;
            }
        }
        printf("ncc: arp clock source %u: %u steps, %u missed grid starts, %u off-grid steps\n",
               source, steps, missed, stray);
        ok("arp clock: actual 1/16 attacks stay on USB/TRS pulses across jitter and tempo changes",
           steps == 256u && !missed && !stray);
    }
}

/* Every rate, including triplets and rates longer than one bar. Full swing puts
 * its delayed step at 1.4 periods; integer pulse positions suffice at the fast
 * rates and interpolation fills the fractional pulse. Also restart while playing. */
static void arp_clock_rates_test(void)
{
    uint32_t rate, swing, k, b, elapsed, base, previous, expected, steps, wrong, period, pulse;
    for (rate = 0; rate < 10u; rate++) {
        for (swing = 0; swing <= 100u; swing += 100u) {
            power();
            song.g[G_CLOCK] = 2;
            blk();
            trk[1].p[P_AMODE] = 1;
            trk[1].p[P_ARATE] = (int16_t)rate;
            trk[1].p[P_ASWING] = (int16_t)swing;
            period = rate < 6u ? 24u / DIV_DEN[rate] : 24u << (rate - 5u);
            base = fm1_ms;
            elapsed = steps = wrong = 0;
            midi_enqueue(0xFA0Fu, 2);
            midi(0x93, 60, 100);
            for (k = 0; k < 2u * 768u; k++) {
                pulse = k % 768u;
                if (k == 768u)
                    midi_enqueue(0xFA0Fu, 2);  /* Start again without a preceding Stop */
                midi_enqueue(0xF80Fu, 2);
                for (b = 0; b < 29u; b++) {
                    fm1_ms = base + elapsed * 1000u / (uint32_t)FS;
                    previous = trk[1].arp_idx;
                    if (!k && !b)
                        previous = 0xFFFFFFFFu;
                    blk();
                    if (trk[1].arp_idx != previous) {
                        uint32_t phase = pulse % (2u * period);
                        uint32_t target = phase < period ? 0u : period * (250u + swing) / 250u;
                        uint32_t target_frac = phase < period ? 0u : period * (250u + swing) % 250u;
                        /* At most one input pulse around a swung fractional boundary;
                         * straight steps must be exactly on the pulse's first block. */
                        wrong += swing ? phase != target && !(target_frac && phase == target + 1u) :
                                         pulse % period != 0u || b != 0u;
                        steps++;
                    }
                    elapsed += CTL;
                }
            }
            expected = 1536u / period;
            printf("ncc: arp rate %u swing %u: %u/%u steps, %u off grid\n", rate, swing, steps, expected, wrong);
            ok("arp clock: all ten rates and swing retain the grid across a repeated Start",
               steps == expected && !wrong);
        }
    }
}

/* The Circuit's own sequencer playing the arp's notes (Remco, 2026-10-27: on time with a held key, drifting once the
 * Circuit's sequencer plays them). A note a 1/16 step, short (2 pulses), long (5) or legato (7: the next note-on before
 * this note-off), on the step or a micro step (pulse) after it, sent before or after its clock; 1/16 and 1/32 arps,
 * both clock sources, the tempo changing each bar. Every attack must be on the first block of a grid pulse, and every
 * grid pulse with a note held must attack. */
static void arp_seq_notes_test(void)
{
    static const uint8_t GATE[3] = {2, 5, 7}, RATE[2] = {2, 3};
    uint32_t source, g, r, micro, order, k, b, elapsed, base, steps, missed, stray, blocks, period, pidx, poff;
    uint32_t bad = 0;
    for (source = 1; source <= 2; source++)
        for (g = 0; g < 3u; g++)
            for (r = 0; r < 2u; r++)
                for (micro = 0; micro < 2u; micro++)
                    for (order = 0; order < 2u; order++) {
                        uint32_t on = 0, offs[2] = {0, 0}, offk[2] = {0, 0};
                        power();
                        song.g[G_CLOCK] = (int16_t)source;
                        blk();
                        trk[1].p[P_AMODE] = 1;
                        trk[1].p[P_ARATE] = RATE[r];
                        period = 24u / DIV_DEN[RATE[r]];
                        base = fm1_ms;
                        elapsed = steps = missed = stray = 0;
                        midi_enqueue(0xFA0Fu, source);
                        for (k = 0; k < 16u * 96u; k++) {
                            uint32_t i, note = k % 6u == micro ? 60u + (k / 6u) % 3u * 2u : 0u;
                            blocks = ((k / 96u) & 1u ? 24u : 34u) + k % 3u;
                            for (i = 0; i < 2u; i++)       /* the sequencer's note-offs at their pulse */
                                if (offs[i] && offk[i] == k) {
                                    midi_enqueue((0x8u | 0x83u << 8 | offs[i] << 16), source);
                                    offs[i] = 0;
                                }
                            if (note && order)
                                midi_enqueue(0xF80Fu, source);
                            if (note) {
                                midi_enqueue((0x9u | 0x93u << 8 | note << 16 | 100u << 24), source);
                                offs[on] = note;
                                offk[on] = k + GATE[g];
                                on ^= 1u;
                            }
                            if (!note || !order)
                                midi_enqueue(0xF80Fu, source);
                            for (b = 0; b < blocks; b++) {
                                fm1_ms = base + elapsed * 1000u / (uint32_t)FS;
                                pidx = trk[1].arp_idx;
                                poff = trk[1].arp_off;
                                blk();
                                /* arp_add seeds arp_idx to its sentinel on every fresh gesture (ncc_seq.c), with no
                                 * attack yet; only count an actual step, not that bookkeeping reset. */
                                if ((trk[1].arp_idx != pidx && trk[1].arp_idx != 0xFFFFFFFFu) || trk[1].arp_off > poff) {
                                    steps++;
                                    stray += k % period != 0u || b != 0u;
                                } else if (!b && !(k % period) && trk[1].nheld)
                                    missed++;
                                elapsed += CTL;
                            }
                        }
                        if (stray || missed) {
                            printf("ncc: arp from the sequencer: source %u gate %u rate %u micro %u order %u: %u steps,"
                                   " %u missed, %u off grid\n", source, GATE[g], RATE[r], micro, order, steps, missed,
                                   stray);
                            bad++;
                        }
                    }
    ok("arp clock: notes from the Circuit's sequencer (short, long, legato, micro steps) keep every attack on the grid",
       !bad);
}
