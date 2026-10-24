/* SPDX-License-Identifier: GPL-3.0-only */
/* NC: exact b*n/d for VOICE's first formant without 64-bit division. In this path b < 65536, b < d < n < 749700
 * (inc is 32-bit, f0=inc*705600>>32). The coarse Q12 quotient uses a 32-bit numerator; after the multiply its
 * remainder is below d + b*d/4096 (<13 million), so the correction is another 32-bit divide. Two multiplies
 * may be wide; neither divide is. This reproduces the old positive integer quotient, not an approximation. */
static uint32_t ncc_formant_bw(uint32_t b, uint32_t n, uint32_t d)
{
    uint32_t q, y, rem;
    if (!d)
        return 0;
    q = (n << 12) / d;
    y = (uint32_t)(((uint64_t)b * q) >> 12);
    rem = (uint32_t)((uint64_t)b * n - (uint64_t)y * d);
    return y + rem / d;
}
