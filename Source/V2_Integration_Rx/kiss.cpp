#include "kiss.h"

static uint8_t update_crc8(uint8_t crc, uint8_t crc_seed)
{
    uint8_t crc_u = crc ^ crc_seed;
    for(uint8_t i = 0; i < 8; i++)
    {
        crc_u = (crc_u & 0x80) ? (0x07 ^ (uint8_t)(crc_u << 1)) : (uint8_t)(crc_u << 1);
    }
    return crc_u;
}

uint8_t kiss_crc8(const uint8_t *buf, uint8_t len)
{
    uint8_t crc = 0;
    for(uint8_t i = 0; i < len; i++)
    {
        crc = update_crc8(buf[i], crc);
    }
    return crc;
}

bool kissDecode(const uint8_t *frame, struct kissFrame *out)
{
    if(kiss_crc8(frame, KISS_FRAME_LEN - 1) != frame[KISS_FRAME_LEN - 1]) return false;

    out->temp     = frame[0];
    out->volt     = ((uint16_t)frame[1] << 8) | frame[2];
    out->current  = ((uint16_t)frame[3] << 8) | frame[4];
    out->used_mah = ((uint16_t)frame[5] << 8) | frame[6];
    out->erpm     = (((uint32_t)frame[7] << 8) | frame[8]) * 100UL;

    return true;
}
