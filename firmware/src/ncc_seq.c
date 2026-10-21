/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC (FELUCCA_LIVEUI=1): the arp on the clock's grid and Drone-hold, for the NCC parts (tracks 1 and 2). Included by
 * seq.c after input_off (it needs the arp's and the notes' functions); the audio ISR only, but for the requests the
 * main loop writes (ncd_want, live_ui.c).
 *
 * THE ARP ON THE GRID. Felucca's arp starts at the first note and counts from there; NCC's starts on the next step of
 * the clock's grid: the step (its RATE) the bar is in (ncc_bar: samples into the bar, seq.c beat_pos / beat_n, which
 * the external clock's Start puts at 0), so a 1/16 arp plays the 1/16s of the Circuit's bar. A note at most an eighth
 * of a step late still starts at once (played live a little late, not a step late). The transport's Start (playing
 * from 0 to 1) starts every held arp from its first step together with the bar; its Stop (1 to 0: the Circuit's
 * sequence stopped) stops them: the held and latched notes are dropped (HOLD too) and the sounding arp note ends.
 * With no clock coming (seq.c events_block) the arp runs on the tempo (BPM), so it plays without the Circuit too.
 *
 * DRONE-HOLD (SEQ, per part). While on, a note's note-off is not let through: it rings on. More notes pile up (to the
 * voice cap, the engine's stealing as always); a note that drones played again lets that note go; Drone-hold off, or
 * the transport's Stop (Drone-hold stays on for what comes next), lets every droning note go through its release. With the arp on, the arp holds the droning notes as it holds pressed
 * ones. The macros keep working on them. */

#define NCD_N 16u                       /* droning notes per part */
static volatile uint8_t ncd_want[NCM_PARTS];   /* main: Drone-hold on (SEQ) */
static struct {
    uint8_t on, n;                      /* the ISR's own copy of ncd_want; the notes droning */
    uint8_t note[NCD_N];
    uint8_t hold;                       /* P_AHOLD as last seen (HOLD off: the latched notes go) */
} ncd[NCM_PARTS];
static uint32_t ncc_bar;                /* samples into the bar (at this block's start) */
static uint8_t ncc_playing;             /* song.playing as last seen */

static int ncd_find(uint32_t part, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < ncd[part].n; i++)
        if (ncd[part].note[i] == note)
            return (int)i;
    return -1;
}

/* input_on: 1 when the note drones already (then let go here); else it is kept as droning */
static int ncd_again(track_t *t, uint32_t note)
{
    uint32_t part = trk_index(t);
    int i;
    if (part >= NCM_PARTS || !(ncd[part].on | ncd_want[part]))   /* (asked for: from this note on, before the block's */
        return 0;                                                   /* ncc_seq_block has seen it) */
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
    return part < NCM_PARTS && (ncd[part].on | ncd_want[part]) && ncd_find(part, note) >= 0;
}

/* arp_add, the first note held: where the arp is in its step (arp_pos), so it fires on the grid */
static uint32_t ncc_arp_phase(const track_t *t)
{
    uint32_t p = div_samples((uint32_t)t->p[P_ARATE]), ph;
    if (trk_index(t) >= NCM_PARTS || !p)
        return 0xFFFFFFFu;                              /* (seq.c: fire on this block) */
    ph = ncc_bar % p;
    return ph < p / 8u ? 0xFFFFFFFu : ph;               /* a little late: at once */
}

/* part's droning notes through their release (one the Circuit still holds: its own note-off) */
static void ncd_release(uint32_t part)
{
    track_t *t = &trk[part];
    uint32_t i;
    for (i = 0; i < ncd[part].n; i++)
        if (!midi_note_held(t, ncd[part].note[i])) {
            arp_remove(t, ncd[part].note[i]);
            trk_note_off(t, ncd[part].note[i]);
        }
    ncd[part].n = 0;
}

/* events_block, before the arps step: Drone-hold off, HOLD off, the transport's Start and Stop, the bar's place */
static void ncc_seq_block(void)
{
    uint32_t part;
    ncc_bar = beat_n * beat_samples() + beat_pos;
    for (part = 0; part < NCM_PARTS; part++) {
        track_t *t = &trk[part];
        if (ncd[part].on != ncd_want[part]) {
            ncd[part].on = ncd_want[part];
            if (!ncd[part].on)
                ncd_release(part);                      /* every droning note through its release */
        }
        if (!song.playing && ncc_playing) {             /* Stop: the arp and the drone stop */
            ncd_release(part);
            t->nheld = 0;
            if (t->arp_note || t->arp_ch[0])
                arp_off(t);
        }
        if (ncd[part].hold && !t->p[P_AHOLD] && !t->arp_phys && !ncd[part].on)
            t->nheld = 0;                               /* HOLD off with no key down: the latched chord goes */
        ncd[part].hold = (uint8_t)(t->p[P_AHOLD] != 0);
        if (song.playing && !ncc_playing && t->p[P_AMODE] && t->nheld) {
            t->arp_pos = 0xFFFFFFFu;                    /* Start: from the first step, with the bar */
            t->arp_idx = 0xFFFFFFFFu;
        }
    }
    ncc_playing = song.playing;
}
