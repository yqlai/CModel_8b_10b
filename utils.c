# include "headers/utils.h"

uint8_t calculateHEC(uint32_t header){
    uint8_t data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = (header >> (24 - i * 8)) & 0xFF;
    }

    uint8_t crc = 0;
    for (int i = 0; i < 3; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ ECC_POLY;
            else
                crc <<= 1;
        }
    }
    crc ^= ECC_XOROUT;
    return crc;
}

uint8_t calculateECC(uint32_t header) {
    uint8_t data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = (header >> (24 - i * 8)) & 0xFF;
    }

    uint8_t crc = 0;
    for (int i = 1; i < 4; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ ECC_POLY;
            else
                crc <<= 1;
        }
    }
    crc ^= 0x00; // XOROUT value is defined as 0x00 for the ECC calculation.
    return crc;
}

// Utility function to convert a string of hex digits into a byte array
void hexStringToBytes(const char* hexString, unsigned char* byteArray) {
    while (*hexString) {
        sscanf(hexString, "%2hhx", byteArray++);
        hexString += 2;
    }
}

// Utility function to convert byte array back to hex string
void bytesToHexString(const unsigned char* byteArray, int length, char* hexString, FILE* file, int isHeader) {
    static int ind_SDP = 0;
    for (int i = 0; i < length; i++) {
        sprintf(hexString + (i * 2), "%02X", byteArray[i]);
        if(file)
        {
            if (ind_SDP % 4 == 0)
            {
                fprintf(file, "  %02X", 1);
                fprintf(file, "  %02X", isHeader);
            }

            fprintf(file, "  %02X", byteArray[i]);
            ind_SDP++;

            if (ind_SDP % 4 == 0)
                fprintf(file, "\n");
        }
    }
    if (isHeader == 1) ind_SDP = 0;
}

// Fill the payload with incremental values (00, 01, 02, etc.)
void fillPayload(unsigned char* payload, int length) {
    for (int i = 0; i < length; i++) {
        payload[i] = i & 0xFF;
    }
}