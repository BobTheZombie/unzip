#include <assert.h>
#include <stdio.h>
#include <zlib.h>
#include "../modern/crc-fast.h"
int main(void)
{
    unsigned char data[65552];
    uint32_t r = 1;
    for (size_t i = 0; i < sizeof data; i++) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        data[i] = (unsigned char)r;
    }
    assert(uz_crc32_fast(123, NULL, 42) == 0);
    assert(uz_crc32_fast(0, (const unsigned char *)"123456789", 9) == 0xcbf43926);
    for (size_t offset = 0; offset < 16; offset++) {
        for (size_t n = 0; n <= 1024; n++) {
            assert(uz_crc32_fast(0, data + offset, n) == crc32(0, data + offset, (uInt)n));
            assert(uz_crc32_fast(0xabcdef01u, data + offset, n) == crc32(0xabcdef01u, data + offset, (uInt)n));
        }
        for (size_t n = 1025; n <= 65536; n += 127) {
            uint32_t c = uz_crc32_fast(0, data + offset, n / 2);
            c = uz_crc32_fast(c, data + offset + n / 2, n - n / 2);
            assert(c == crc32(0, data + offset, (uInt)n));
        }
    }
    puts("CRC: reference, alignment, tail, seed and streaming checks passed");
    return 0;
}
