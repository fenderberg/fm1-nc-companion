/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the arp on the clock's grid and Drone-hold, for the NCC parts (tracks 1 and 2). Included by
 * seq.c after input_off (it needs the arp's and the notes' functions); the audio ISR only, but for the requests the
 * main loop writes (ncd_want, live_ui.c).
 *
 * THE ARP ON THE GRID. Felucca's arp starts at the first note and counts from there; NCC's starts on the next step of
 * the clock's grid: the step (its RATE) the bar is in (ncc_bar: samples into the bar), so a 1/16 arp plays the 1/16s of
 * the Circuit's bar. On the external clock the bar is counted in its pulses from its Start (24 a beat, 96 a bar: where
 * the Circuit's bar is, whatever the tempo's measurement does between them; before 0.3 it was seq.c's beat_pos in
 * samples at the measured tempo, which drifted off the Circuit's bar). Every external-clock attack now follows
 * that pulse grid too (ncc_arp_due), including triplets, long rates and swing: resetting a sample timer after each
 * attack used to lose its overshoot and let tempo measurements move the arp off-grid. Without external clock,
 * the grid uses samples at the BPM (beat_n, beat_pos). A note at most an eighth
 * of a step late still starts at once (played live a little late, not a step late). The transport's Start (playing
 * from 0 to 1, or another MIDI Start) starts every held arp from its first step together with the bar; its Stop (1 to 0: the Circuit's
 * sequence stopped) stops them: the held and latched notes are dropped (HOLD too) and the sounding arp note ends.
 * With no clock coming (seq.c events_block) the arp runs on the tempo (BPM), so it plays without the Circuit too.
 *
 * DRONE-HOLD (SEQ, per part). While on, a note's note-off is not let through: it rings on. More notes pile up (to the
 * voice cap, the engine's stealing as always); a note that drones played again lets that note go; Drone-hold off, or
 * the transport's Stop (Drone-hold stays on for what comes next), lets every droning note go through its release. With the arp on, the arp holds the droning notes as it holds pressed
 * ones. REPLACE keeps a whole gesture: add notes while any physical key is down; once all are released, the next
 * strike replaces the held chord. Retire old arp/mono input; spare POLY voices overlap a ~250 ms outgoing tail
 * with an ~80 ms entrance. Without enough slots, fade the old chord out and the queued notes in (~250 ms each way).
 * The macros keep working on both modes. */

#define NCD_N 16u                       /* droning notes per part */
static volatile uint8_t ncd_want[NCM_PARTS];   /* main: Drone-hold on (SEQ) */
static uint32_t ncd_retired[NCM_PARTS][4];     /* replaced input notes: their late offs must not touch a new arp */
static uint8_t ncd_starting;
static uint8_t ncd_new[NCM_PARTS];           /* first tone of a fresh physical gesture (chord expansion counts once) */
static struct {
    uint8_t on, n;                      /* the ISR's own copy of ncd_want; the notes droning */
    uint8_t note[NCD_N];
    uint8_t hold;                       /* P_AHOLD as last seen (HOLD off: the latched notes go) */
    uint8_t vel[NCD_N], key[NCD_N], replace;
    uint16_t wait;
    uint16_t pending;                   /* queued tones after the musical fade (MONO too) */
} ncd[NCM_PARTS];
static uint32_t ncc_bar;                /* samples into the bar (at this block's start) */
static uint32_t ncc_pulse, ncc_since;   /* the external clock's pulses since its Start (the first is 0), the samples the
                                         * sequencer ran since the last one (midi_clock.c sets them, ncc_seq_block adds) */
static uint8_t ncc_playing;             /* song.playing as last seen */
static uint8_t ncc_arp_clock;
static uint8_t ncc_arp_restart;         /* MIDI Start, including while already playing */
static struct {
    uint32_t slot;
    uint8_t valid, rate, swing;
} ncc_arp_grid[NCM_PARTS];

static int ncd_find(uint32_t part, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < ncd[part].n; i++)
        if (ncd[part].note[i] == note)
            return (int)i;
    return -1;
}

/* input_on: 1 when the note drones already (then let go here); else it is kept as droning */
static int ncd_retrigger(const track_t *t)
{
    uint32_t part = trk_index(t);
    return part < NCM_PARTS && ncd_mode(part) && (ncd[part].on | ncd_want[part]);
}
static void ncd_begin(const track_t *t)
{
    uint32_t part = trk_index(t);
    if (part < NCM_PARTS)
        ncd_new[part] = (uint8_t)!ncc_physical_held(part);
}
/* Canceling a queued gesture must also stop its still-rendering predecessor. Restore the gain gradually;
 * panic/routing/soundload already request a one-block voice kill and keep their ordinary timing. */
static void ncd_abort_fade(uint32_t part)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        if (ncd_overlap[part][i].phase == NCD_FADE_OUT && trk[part].v[i].active)
            voice_kill(&trk[part].v[i]);
        ncd_overlap[part][i].phase = NCD_FADE_IDLE;
    }
    ncd_overlapping[part] = 0;
    if (ncd_fade[part].phase != NCD_FADE_OUT)
        return;
    for (i = 0; i < NVOICE; i++)
        if (trk[part].v[i].active)
            voice_kill(&trk[part].v[i]);
    ncd_fade_in(part);
}
static void ncd_forget(uint32_t part)
{
    uint32_t i;
    ncd_abort_fade(part);
    ncd[part].n = ncd[part].pending = ncd[part].replace = ncd[part].wait = ncd_new[part] = 0;
    for (i = 0; i < 4u; i++)
        ncd_retired[part][i] = 0;
}
static void ncd_replace(track_t *t, uint32_t part)
{
    uint32_t i, note, sounding = 0;
    for (i = 0; i < ncd[part].n; i++) {
        note = ncd[part].note[i];
        ncd_retired[part][note >> 5] |= 1u << (note & 31u);
    }
    for (i = 0; i < t->nheld; i++) {
        note = t->held[i];
        ncd_retired[part][note >> 5] |= 1u << (note & 31u);
    }
    t->nheld = t->arp_phys = t->nmono = t->mono_note = t->xp_n = 0;
    arp_forget(t);
    /* Gather this block's chord before choosing overlap or the capacity fallback. */
    ncd[part].replace = 1;
    ncd[part].wait = (uint16_t)(ncd_fade[part].phase == NCD_FADE_OUT ? ncd_fade_out(part) : 1u);
    for (i = 0; i < NVOICE; i++)
        sounding |= t->v[i].active;
    if (sounding && (!ncd_overlap_room(t, 1u) || ncd_fade[part].phase)) {
        ncd[part].replace = 0;
        ncd[part].wait = (uint16_t)ncd_fade_out(part);
    }
    ncd[part].n = ncd[part].pending = 0;
}
static int ncd_again(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t part = trk_index(t);
    int i;
    if (ncd_starting || part >= NCM_PARTS || !(ncd[part].on | ncd_want[part]))   /* (asked for: from this note on, before the block's */
        return 0;                                                   /* ncc_seq_block has seen it) */
    ncd_retired[part][note >> 5] &= ~(1u << (note & 31u));
    if (ncd_mode(part)) {
        if (ncd_new[part] || !ncd[part].n) {
            ncd_replace(t, part);
        }
        ncd_new[part] = 0;
        ncd_retired[part][note >> 5] &= ~(1u << (note & 31u));
        i = ncd_find(part, note);
        if (i < 0) {
            if (ncd[part].n >= NCD_N)
                return 1;                              /* bounded chord; never start an untracked drone */
            i = ncd[part].n++;
            ncd[part].note[i] = (uint8_t)note;
        }
        if (ncd[part].wait || ncd[part].pending || ncd_overlapping[part] || ncd_fade[part].phase == NCD_FADE_OUT) {
            ncd[part].pending |= (uint16_t)(1u << i);
            ncd[part].vel[i] = (uint8_t)vel;
            ncd[part].key[i] = in_key;
            return 1;
        }
        return 0;                                      /* add/restrike a tone in the current gesture */
    }
    if ((i = ncd_find(part, note)) >= 0) {
        ncd[part].note[i] = ncd[part].note[--ncd[part].n];
        arp_remove(t, note);
        trk_note_off(t, note);
        return 1;
    }
    if (ncd[part].n < NCD_N)
        ncd[part].note[ncd[part].n++] = (uint8_t)note;
    return 0;
}

/* input_off: 1 when Drone-hold keeps the note */
static int ncd_holds(const track_t *t, uint32_t note)
{
    uint32_t part = trk_index(t);
    if (part >= NCM_PARTS)
        return 0;
    if ((ncd_retired[part][note >> 5] >> (note & 31u)) & 1u) {
        ncd_retired[part][note >> 5] &= ~(1u << (note & 31u));
        return 1;
    }
    return (ncd[part].on | ncd_want[part]) && (ncd_mode(part) || ncd_find(part, note) >= 0);
}

/* arp_add, the first note held: where the arp is in its step (arp_pos), so it fires on the grid */
static uint32_t ncc_arp_phase(const track_t *t)
{
    uint32_t p = div_samples((uint32_t)t->p[P_ARATE]), ph;
    if (trk_index(t) >= NCM_PARTS || !p)
        return 0xFFFFFFFu;                              /* (seq.c: fire on this block) */
    ncc_arp_grid[trk_index(t)].valid = 0;
    ph = ncc_bar % p;
    return ph < p / 8u ? 0xFFFFFFFu : ph;               /* a little late: at once */
}

/* Two steps of the longest rate (4BAR) fit in 768 MIDI clocks. Q8 pulse position keeps
 * straight/triplet boundaries exact and interpolates swing between pulses. Crucially,
 * measured BPM changes cannot move those boundaries, and no block overshoot is lost.
 * One slot change emits at most one note: a delayed packet burst never makes a burst
 * of catch-up notes. This state belongs only to NCC's two parts, in the audio ISR. */
static int ncc_arp_due(const track_t *t)
{
    uint32_t part = trk_index(t), rate, period, ps, frac, pos, pair, within, split, slot;
    int fresh, due;
    if (part >= NCM_PARTS)
        return -1;
    if (!ncc_arp_clock || !t->p[P_AMODE] || !t->nheld) {
        ncc_arp_grid[part].valid = 0;
        return -1;
    }
    rate = (uint32_t)t->p[P_ARATE];
    period = rate < 6u ? 6144u / DIV_DEN[rate] : rate < 10u ? 6144u << (rate - 5u) : 6144u / DIV_DEN[rate % 6u];
    ps = beat_samples() / 24u;
    frac = ps ? ncc_since * 256u / ps : 0u;
    if (frac > 255u)
        frac = 255u;                                /* never predict a clock before it arrives */
    pos = (ncc_pulse % 768u) * 256u + frac;
    pair = period * 2u;
    within = pos % pair;
    split = period + (uint32_t)t->p[P_ASWING] * period / 250u;
    slot = pos / pair * 2u + (within >= split);
    fresh = !ncc_arp_grid[part].valid || ncc_arp_grid[part].rate != rate ||
            ncc_arp_grid[part].swing != t->p[P_ASWING];
    due = fresh ? (within >= split ? within - split : within) < period / 8u :
                  slot != ncc_arp_grid[part].slot;
    ncc_arp_grid[part].valid = 1;
    ncc_arp_grid[part].rate = (uint8_t)rate;
    ncc_arp_grid[part].swing = (uint8_t)t->p[P_ASWING];
    ncc_arp_grid[part].slot = slot;
    return due;
}

/* part's droning notes through their release (one the Circuit still holds: its own note-off) */
static void ncd_release(uint32_t part)
{
    track_t *t = &trk[part];
    uint32_t i, keep = 0;
    for (i = 0; i < ncd[part].n; i++) {
        if (!midi_note_held(t, ncd[part].note[i]) && !midi_local_held(t, ncd[part].note[i])) {
            arp_remove(t, ncd[part].note[i]);
            trk_note_off(t, ncd[part].note[i]);
        } else if (ncd[part].pending >> i & 1u) {
            ncd[part].note[keep] = ncd[part].note[i];
            ncd[part].vel[keep] = ncd[part].vel[i];
            ncd[part].key[keep++] = ncd[part].key[i];
        }
    }
    ncd[part].n = (uint8_t)keep;
    ncd[part].pending = (uint16_t)((1u << keep) - 1u);  /* Drone off during a fade: queued physical tones still play */
    ncd_new[part] = 0;
    if (!ncd[part].pending)
        ncd_abort_fade(part);                            /* no physical tone survived Drone off */
}

/* events_block, before the arps step: Drone-hold off, HOLD off, the transport's Start and Stop, the bar's place */
static void ncc_seq_block(uint32_t clock_mode, uint32_t seq_n)
{
    uint32_t part, ps = beat_samples() / 24u;
    ncc_arp_clock = (uint8_t)(clock_mode && song.playing);
    if (clock_mode && song.playing && ps) {             /* the pulse the bar is at, and how far past it */
        ncc_bar = (ncc_pulse % 96u) * ps + (ncc_since < ps ? ncc_since : ps - 1u);
        ncc_since += seq_n;
    } else
        ncc_bar = beat_n * beat_samples() + beat_pos;
    for (part = 0; part < NCM_PARTS; part++) {
        track_t *t = &trk[part];
        ncd_fade_tick(part);                             /* one writer, before either render core reads it */
        ncd_overlap_tick(part);
        if (ncd[part].on != ncd_want[part]) {
            ncd[part].on = ncd_want[part];
            if (!ncd[part].on)
                ncd_release(part);                      /* every droning note through its release */
        }
        if (!song.playing && ncc_playing) {             /* Stop: the arp and the drone stop */
            ncd_release(part);
            ncd_forget(part);
            t->nheld = 0;
            if (t->arp_note || t->arp_ch[0])
                arp_off(t);
        }
        if (ncd[part].hold && !t->p[P_AHOLD] && !t->arp_phys && !ncd[part].on)
            t->nheld = 0;                               /* HOLD off with no key down: the latched chord goes */
        ncd[part].hold = (uint8_t)(t->p[P_AHOLD] != 0);
        if (song.playing && (!ncc_playing || ncc_arp_restart) && t->p[P_AMODE] && t->nheld) {
            ncc_arp_grid[part].valid = 0;
            t->arp_pos = 0xFFFFFFFu;                    /* Start: from the first step, with the bar */
            t->arp_idx = 0xFFFFFFFFu;
        }
        if (ncd[part].pending) {
            if (ncd[part].wait)
                ncd[part].wait--;
            else {
                uint8_t key = in_key;
                uint32_t i, pending = ncd[part].pending, count = 0, sounding = 0;
                for (i = 0; i < ncd[part].n; i++)
                    count += (pending >> i) & 1u;
                for (i = 0; i < NVOICE; i++)
                    sounding |= t->v[i].active;
                if (ncd[part].replace && ncd_fade[part].phase != NCD_FADE_OUT) {
                    ncd[part].replace = 0;
                    if (sounding) {
                        if (ncd_fade[part].phase == NCD_FADE_IDLE && ncd_overlap_room(t, count))
                            ncd_overlap_begin(part);
                        else {
                            ncd[part].wait = (uint16_t)ncd_fade_out(part);
                            continue;                  /* fade the old chord, never steal it in one block */
                        }
                    }
                } else if (ncd_fade[part].phase != NCD_FADE_OUT && ncd_overlapping[part] &&
                           !ncd_overlap_room(t, count))
                    continue;                          /* chord expansion waits for retiring slots */
                ncd[part].pending = 0;
                ncd[part].replace = 0;
                if (ncd_fade[part].phase == NCD_FADE_OUT) {
                    /* The previous block reached exactly zero. Slots (including PHYS/GRAIN and MONO) can now
                     * be reused without overlap, extra voice cost, or a discontinuity in their output. */
                    uint32_t v;
                    for (v = 0; v < NVOICE; v++) {
                        t->v[v].active = t->v[v].gate = t->v[v].stage = 0;
                        t->v[v].env = t->v[v].env_out = 0;
                        ncd_overlap[part][v].phase = NCD_FADE_IDLE;
                    }
                    ncd_overlapping[part] = 0;
                    ncd_fade_in(part);
                }
                ncd_starting = 1;
                for (i = 0; i < ncd[part].n; i++)
                    if (pending >> i & 1u) {
                        in_key = ncd[part].key[i];
                        input_on(t, ncd[part].note[i], ncd[part].vel[i]);
                    }
                ncd_starting = 0;
                in_key = key;
                if (!(ncd[part].on | ncd_want[part]))
                    ncd[part].n = 0;
            }
        }
    }
    ncc_playing = song.playing;
    ncc_arp_restart = 0;
}
