/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC Dual: only notes fan out. Two private MIDI ownership slots (16=L, 17=R) reuse the normal chord, pedal,
 * release and panic bookkeeping without adding either part to a physical channel's controller targets. The slots
 * never enter the USB/TRS queue or MIDI OUT. Local keys keep separate snapshots, so focus, chord or patch changes
 * cannot send their note-offs to another part. All state here belongs to the audio ISR. */
typedef struct { uint8_t n, ch, note[CHORD_MAX]; } ncc_key_t;
static ncc_key_t ncc_keys[27][NCM_PARTS];
static uint32_t ncc_key_mask;

static int ncc_local_held(uint32_t part, uint32_t note)
{
    uint32_t k, j;
    if (part >= NCM_PARTS || !ncc_key_mask)
        return 0;
    for (k = 0; k < 27u; k++)
        for (j = 0; j < ncc_keys[k][part].n; j++)
            if (ncc_keys[k][part].note[j] == note)
                return 1;
    return 0;
}
static int ncc_local_any(uint32_t part)
{
    uint32_t k;
    if (part < NCM_PARTS)
        for (k = 0; k < 27u; k++)
            if (ncc_keys[k][part].n)
                return 1;
    return 0;
}
static int ncc_physical_held(uint32_t part)
{
    uint32_t k;
    if (part >= NCM_PARTS)
        return 0;
    if (ncc_midi_down[part] || ncc_local_any(part))
        return 1;
    for (k = 0; k < 27u; k++)
        if (kb_chn[k] && kb_trk[k] == part)
            return 1;
    return 0;
}
static void ncc_key_on(uint32_t k)
{
    uint32_t part, j;
    ncc_key_mask |= 1u << k;
    kb_chn[k] = 0;                            /* the ordinary key snapshot is not an extra owner */
    for (part = 0; part < NCM_PARTS; part++) {
        track_t *t = &trk[part];
        ncc_key_t *s = &ncc_keys[k][part];
        uint8_t nn[CHORD_MAX];
        uint32_t n = chord_build(t, kb_note[k], nn);
        s->n = 0;
        ncd_begin(t);
        s->ch = (uint8_t)trk_midi_ch(part);
        in_key = (uint8_t)(k + 1u);
        for (j = 0; j < n; j++) {
            if ((!midi_local_held(t, nn[j]) && !midi_note_held(t, nn[j])) || ncd_retrigger(t)) {
                input_on(t, nn[j], kb_vel);
                midi_out_event(0x09u | (0x90u | s->ch) << 8 | (uint32_t)nn[j] << 16 | (uint32_t)kb_vel << 24);
            }
            s->note[j] = nn[j];
        }
        s->n = (uint8_t)n;
    }
    in_key = 0;
    last_note = kb_note[k];
}
static int ncc_key_off(uint32_t k)
{
    uint32_t part, j;
    if (!(ncc_key_mask >> k & 1u))
        return 0;
    ncc_key_mask &= ~(1u << k);
    for (part = 0; part < NCM_PARTS; part++) {
        ncc_key_t *s = &ncc_keys[k][part];
        uint32_t n = s->n;
        s->n = 0;
        for (j = 0; j < n; j++)
            if (!midi_local_held(&trk[part], s->note[j])) {
                input_off(&trk[part], s->note[j]);
                midi_out_event(0x08u | (0x80u | s->ch) << 8 | (uint32_t)s->note[j] << 16);
            }
    }
    return 1;
}
static void ncc_input_forget(uint32_t part)
{
    uint32_t k, j;
    if (part >= NCM_PARTS)
        return;
    ncd_forget(part);
    ncc_midi_down[part] = 0;
    for (k = 0; k < 27u; k++) {
        ncc_key_t *s = &ncc_keys[k][part];
        for (j = 0; j < s->n; j++)
            midi_out_event(0x08u | (0x80u | s->ch) << 8 | (uint32_t)s->note[j] << 16);
        s->n = 0;
    }
}
static void ncc_input_stop(void)
{
    uint32_t part, k;
    for (part = 0; part < NCM_PARTS; part++) {
        int32_t bend = midi_bend_target[part], cur = midi_bend_q8[part];
        midi_silence_track(part);              /* fades voices; also forgets private/local owners and Drone notes */
        midi_bend_target[part] = bend;         /* routing changes do not reset the part's controllers */
        midi_bend_q8[part] = cur;
    }
    for (k = 0; k < 27u; k++)
        if ((fm1_in.notes >> k & 1u) && !(kb_layer >> k & 1u)) {
            if (kb_chn[k])
                key_off(k, &trk[kb_trk[k] % NTRK]);   /* an ordinary key held when Dual was enabled: MIDI OUT off too */
            kb_note[k] = KB_SILENT;
            kb_chn[k] = 0;
        }
    ncc_key_mask = 0;
}
static void ncc_input_block(void)
{
    uint32_t mode = ncc_dual_mode();
    if (mode != ncc_dual) {
        ncc_input_stop();
        ncc_dual = (uint8_t)mode;
    }
}
static int ncc_note_event(uint32_t ch, uint32_t note, uint32_t vel)
{
    uint32_t part;
    if (!ncc_dual)
        return 0;
    if (ch == (ncc_dual == 1u ? 2u : 3u))
        for (part = 0; part < NCM_PARTS; part++)
            midi_note_event_to(&trk[part], 16u + part, note, vel, 0);
    return 1;                                 /* every other channel is silent for notes only */
}
static void ncc_control(uint32_t ch, uint32_t cc, uint32_t value)
{
    uint32_t part, mask;
    if (cc != 64u && cc != 121u && cc != 123u)
        return;
    mask = midi_targets(ch);
    for (part = 0; part < NCM_PARTS; part++)
        if (mask >> part & 1u) {
            midi_control(16u + part, cc, value); /* only this part's private notes, never the other layer */
            if (cc == 123u && !midi_track_held(part))
                ncd_forget(part);              /* includes a REPLACE note waiting for its fade */
        }
}
