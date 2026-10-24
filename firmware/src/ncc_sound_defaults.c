/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC: choosing any sound starts its delay/reverb sends at zero, after all preset values have been applied.
 * A project restore, undo, controller or ordinary parameter edit is not a sound choice. Keep the shared buses and
 * their tails; only the loaded part's sends change. This flag follows the outermost, possibly nested UI load. */
static uint8_t ncc_load_sound;
static void ncc_sends_zero(track_t *t) { t->p[P_DLY] = t->p[P_REV] = 0; }
static void ncc_sound_defaults(track_t *t)
{
    if (ncc_load_sound) {
        ncc_sends_zero(t);
        ncc_load_sound = 0;
    }
}
