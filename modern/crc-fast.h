#ifndef UZ_CRC_FAST_H
#define UZ_CRC_FAST_H
#include <stdint.h>
#include <stddef.h>
#include "crc-table.h"

/* Byte loads are endian independent and never read beyond the input. */
static uint32_t uz_crc32_fast(uint32_t crc, const unsigned char *p, size_t n)
{
    uint32_t c = ~crc;
    if (!p) return 0;
    while (n >= 8) {
        uint32_t a = c ^ ((uint32_t)p[0] | (uint32_t)p[1] << 8 |
                          (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
        c = uz_crc_table[7][a & 255] ^ uz_crc_table[6][(a >> 8) & 255] ^
            uz_crc_table[5][(a >> 16) & 255] ^ uz_crc_table[4][a >> 24] ^
            uz_crc_table[3][p[4]] ^ uz_crc_table[2][p[5]] ^
            uz_crc_table[1][p[6]] ^ uz_crc_table[0][p[7]];
        p += 8;
        n -= 8;
    }
    while (n--) c = (c >> 8) ^ uz_crc_table[0][(c ^ *p++) & 255];
    return ~c;
}
#endif
