// Host test for the KISS frame decoder. Build and run:
//   c++ -std=c++17 -o /tmp/test_kiss test/test_kiss.cpp Source/V2_Integration_Rx/kiss.cpp && /tmp/test_kiss

#include "../Source/V2_Integration_Rx/kiss.h"
#include <cstdio>
#include <cstring>

static int failures = 0;

static void check(bool ok, const char *what)
{
    printf("%-58s %s\n", what, ok ? "ok" : "FAILED");
    if(!ok) failures++;
}

// Builds a frame the way an ESC would, CRC included
static void makeFrame(uint8_t *f, uint8_t temp, uint16_t volt, uint16_t cur,
                      uint16_t mah, uint16_t erpm100)
{
    f[0] = temp;
    f[1] = volt >> 8;    f[2] = volt & 0xFF;
    f[3] = cur >> 8;     f[4] = cur & 0xFF;
    f[5] = mah >> 8;     f[6] = mah & 0xFF;
    f[7] = erpm100 >> 8; f[8] = erpm100 & 0xFF;
    f[9] = kiss_crc8(f, 9);
}

int main()
{
    uint8_t f[KISS_FRAME_LEN];
    struct kissFrame out;

    // 1. A well formed frame decodes and the scaling is right
    // 42 degC, 50.40V, 12.34A, 1500mAh, 12300 eRPM
    makeFrame(f, 42, 5040, 1234, 1500, 123);
    memset(&out, 0, sizeof(out));
    check(kissDecode(f, &out), "valid frame is accepted");
    check(out.temp == 42, "  temperature");
    check(out.volt == 5040, "  voltage in 0.01V");
    check(out.current == 1234, "  current in 0.01A");
    check(out.used_mah == 1500, "  consumption in mAh");
    check(out.erpm == 12300, "  eRPM, raw value times 100");

    // 2. Big endian, not little endian: a value whose bytes differ
    makeFrame(f, 0, 0x1234, 0, 0, 0);
    kissDecode(f, &out);
    check(out.volt == 0x1234, "16 bit values are big endian");

    // 3. Every single flipped bit has to be caught
    int missed = 0;
    for(int byte = 0; byte < KISS_FRAME_LEN; byte++)
    {
        for(int bit = 0; bit < 8; bit++)
        {
            makeFrame(f, 42, 5040, 1234, 1500, 123);
            f[byte] ^= (1 << bit);
            if(kissDecode(f, &out)) missed++;
        }
    }
    check(missed == 0, "all 80 single bit errors are rejected");

    // 4. The struct is left alone when the frame is bad
    makeFrame(f, 42, 5040, 1234, 1500, 123);
    f[3] ^= 0x01;
    out.temp = 7;
    out.volt = 7;
    check(!kissDecode(f, &out), "broken frame is rejected");
    check(out.temp == 7 && out.volt == 7, "  and the previous values survive");

    // 5. Extremes do not wrap around
    makeFrame(f, 255, 65535, 65535, 65535, 65535);
    check(kissDecode(f, &out), "maximum values decode");
    check(out.erpm == 6553500UL, "  eRPM does not overflow 16 bit");

    // 6. A misaligned window is what the CRC cannot be trusted with, this
    //    documents why KISS.ino frames by the pause and not by a sliding CRC
    uint8_t stream[256];
    for(int i = 0; i < 256; i++) stream[i] = (uint8_t)(i * 37 + 11);
    int falseHits = 0;
    for(int p = 0; p + KISS_FRAME_LEN <= 256; p++)
    {
        if(kissDecode(stream + p, &out)) falseHits++;
    }
    printf("%-58s %d\n", "false CRC hits over 247 arbitrary windows", falseHits);

    printf("\n%s\n", failures ? "TESTS FAILED" : "all tests passed");
    return failures ? 1 : 0;
}
