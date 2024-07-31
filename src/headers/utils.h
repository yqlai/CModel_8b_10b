#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef __UTILS_H
#define __UTILS_H

#define ECC_POLY 0x07
#define ECC_XOROUT 0x55
#define HOPID_DEFAULT 0

enum PAYLOAD_TYPE {
    ZERO,
    VBID,
    MVID,
    MAUD
};

uint8_t calculateHEC(uint32_t);
uint8_t calculateECC(uint32_t);

void bytesToHexString(const unsigned char*, int, FILE*, int);

void fillPayload(unsigned char*, int);

#endif