/* SPDX-License-Identifier: GPL-3.0-only */
/* NCC: smp_user_scan validates each user zone, then rebases its offset from the slot onto SMP_DATA. Such an offset
 * addresses a separate XIP region (a separate RAM image in host tests), not an element of the built-in SMP_DATA
 * array. Reconstruct the address as an integer before dereferencing it: indexing that array past its end is
 * undefined even when the resulting address holds a valid user sample. The target's 32-bit address arithmetic is
 * the same as before; host slot images must lie above SMP_DATA within 4 GiB, as required by the rebased offsets. */
static inline uint8_t smp_data_byte(uint32_t off)
{
    return *(const uint8_t *)((uintptr_t)SMP_DATA + off);
}
