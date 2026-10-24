/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC performance choices in G_SYNC: Felucca's retired placeholder (never read). A marked value distinguishes
 * these bits from old projects: 0..1 Dual OFF/CH3/CH4, 2/3 Drone STACK/REPLACE per part, 4 remembered CH4 source.
 * Older marked values still work; an active source takes precedence over the remembered bit. No format change.
 * Main loop is the only writer; the ISR applies Dual changes at a block boundary. Active notes are never stored. */
#define NCC_PERF_MARK 64u
static uint32_t ncc_options(void)
{
    uint32_t v = (uint16_t)song.g[G_SYNC];
    return (v & ~31u) == NCC_PERF_MARK && (v & 3u) < 3u ? v & 31u : 0u;
}
static uint32_t ncc_dual_mode(void) { return ncc_options() & 3u; }
static uint32_t ncc_dual_source(void) { return ncc_dual_mode() ? ncc_dual_mode() : 1u + (ncc_options() >> 4 & 1u); }
static uint32_t ncd_mode(uint32_t part) { return ncc_options() >> (2u + part) & 1u; }
static void ncc_set_dual(uint32_t mode)
{
    uint32_t source = mode && mode < 3u ? mode : ncc_dual_source();
    song.g[G_SYNC] = (int16_t)(NCC_PERF_MARK | (ncc_options() & ~19u) | (source == 2u ? 16u : 0u) |
                             (mode < 3u ? mode : 0u));
}
static void ncd_set_mode(uint32_t part, uint32_t mode)
{
    uint32_t bit = 1u << (2u + part);
    song.g[G_SYNC] = (int16_t)(NCC_PERF_MARK | (ncc_options() & ~bit) | (mode ? bit : 0u));
}
static uint8_t ncc_dual;                       /* ISR: the mode currently applied */
static uint16_t ncc_midi_down[NCM_PARTS];      /* physically held MIDI roots, excluding pedal-held notes */
static int ncc_physical_held(uint32_t part);
static int ncc_note_event(uint32_t ch, uint32_t note, uint32_t vel);
static int ncc_local_held(uint32_t part, uint32_t note);
static int ncc_local_any(uint32_t part);
static void ncc_key_on(uint32_t k);
static int ncc_key_off(uint32_t k);
static void ncc_input_forget(uint32_t part);
static void ncc_input_block(void);
static void ncc_input_stop(void);
static void ncc_control(uint32_t ch, uint32_t cc, uint32_t value);
