/* SPDX-License-Identifier: GPL-3.0-or-later */
/* M10.4: the decoder of the built-in library's packed files (builtin.h; the encoder is tools/builtin_pack.py). A byte-oriented LZ77 in
 * the style of LZ4, with a shared dictionary as the history before the first output byte:
 *
 *   a sequence = token, [more literal-length bytes], literals, then (except in the last one) offset u16 LE, [more match-length bytes]
 *   token: high nibble = literal count, low nibble = match length - 4; a nibble of 15 goes on in bytes (each added; 255 = another follows)
 *   offset: 1 .. 65535 back from the next output byte, through the dictionary: history = dictionary then output
 *   the last sequence ends the input and has literals only (it may have none)
 *
 * Every read of the input and every write of the output is checked first: a corrupt pack gives an error code. */
#include "builtin.h"

int bi_unpack(const uint8_t *dict, uint32_t dictlen, const uint8_t *in, uint32_t inlen, uint8_t *out, uint32_t outlen)
{
    const uint8_t *ip = in, *ie = in + inlen;
    uint8_t *op = out, *oe = out + outlen;
    if (inlen > 0xFFFFFFu || outlen > 0xFFFFFFu || dictlen > 0xFFFFFFu)
        return BI_E_INPUT;
    while (ip < ie) {
        uint32_t t = *ip++, ll = t >> 4, ml = (t & 15u) + 4u, off, k, done;
        if (ll == 15u) {
            uint32_t b;
            do {
                if (ip >= ie)
                    return BI_E_INPUT;
                b = *ip++;
                ll += b;
            } while (b == 255u);
        }
        if (ll > (uint32_t)(ie - ip))
            return BI_E_INPUT;
        if (ll > (uint32_t)(oe - op))
            return BI_E_OUTPUT;
        for (k = 0; k < ll; k++)
            *op++ = *ip++;
        if (ip == ie)
            break;
        if (ie - ip < 2)
            return BI_E_INPUT;
        off = (uint32_t)ip[0] | (uint32_t)ip[1] << 8;
        ip += 2;
        if (ml == 19u) {
            uint32_t b;
            do {
                if (ip >= ie)
                    return BI_E_INPUT;
                b = *ip++;
                ml += b;
            } while (b == 255u);
        }
        done = (uint32_t)(op - out);
        if (off == 0 || off > dictlen + done)
            return BI_E_OFFSET;
        if (ml > (uint32_t)(oe - op))
            return BI_E_OUTPUT;
        for (k = 0; k < ml; k++, done++) {
            uint32_t p = dictlen + done - off;   /* (done grows with each byte: a match may overlap its own output) */
            *op++ = p < dictlen ? dict[p] : out[p - dictlen];
        }
    }
    return op == oe ? BI_OK : BI_E_SHORT;
}
