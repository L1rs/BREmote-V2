#ifndef KISS_H
#define KISS_H

#include <stdint.h>

/*
** KISS ESC telemetry frame, as sent by BLHeli_32 and AM32 with auto telemetry
** enabled: 10 byte at 115200 8N1, all 16 bit values big endian.
**
**   byte 0     temperature in degC
**   byte 1..2  voltage in 0.01V
**   byte 3..4  current in 0.01A
**   byte 5..6  consumption in mAh
**   byte 7..8  eRPM divided by 100
**   byte 9     CRC8
**
** Kept free of Arduino types so the host tests under test/ can link it,
** same idea as the vesc_crc pair. The UART side lives in KISS.ino.
*/

#define KISS_FRAME_LEN 10

struct kissFrame {
    uint8_t temp;
    uint16_t volt;      //0.01V
    uint16_t current;   //0.01A
    uint16_t used_mah;
    uint32_t erpm;
};

//CRC8 of the KISS spec, a different polynomial than the esp_crc8 used on the
//radio link, so it gets its own function
uint8_t kiss_crc8(const uint8_t *buf, uint8_t len);

//Checks the CRC and fills out, returns false and leaves out alone if the
//frame does not check out
bool kissDecode(const uint8_t *frame, struct kissFrame *out);

#endif
