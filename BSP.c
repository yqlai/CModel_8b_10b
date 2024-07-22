# include "headers/BSP.h"

uint8_t calculateHEC(uint32_t header) {
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

// Converts a hex string to a byte array
void hexStringToByteArray(const char *hexString, uint8_t *byteArray, size_t arrayLength) {
    const char *pos = hexString;
    for (size_t i = 0; i < arrayLength; i++) {
        sscanf(pos, "%2hhx", &byteArray[i]);
        pos += 2;
    }
}

// Generate a payload sequentially starting from 00
void generatePayload(uint8_t *payload, size_t payloadLength) {
    for (size_t i = 0; i < payloadLength; i++) {
        payload[i] = i & 0xFF; // Keep incrementing from 00
    }
}

uint32_t generate_tunneled_BS_packet_header() {
    uint32_t tunneledPacketHeader = 0;
    tunneledPacketHeader |= (LENGTH_HEADER & 0xFF) << 8;
    tunneledPacketHeader |= (HOPID & 0x7F) << 16;
    tunneledPacketHeader |= (RESERVED_VALUE & 0xF) << 23;
    tunneledPacketHeader |= (SUPPID_TUNNELED_PACKET & 0x1) << 27;
    tunneledPacketHeader |= (PDF_BLANK_START_PACKET & 0xF) << 28;
    tunneledPacketHeader |= (calculateHEC(tunneledPacketHeader) & 0xFF);

    return tunneledPacketHeader;
}

uint32_t generate_BS_packet_header(uint8_t SR, uint8_t CP, uint32_t FillCount)
{
    uint32_t BlankStartHeader = 0;
    BlankStartHeader |= (CP & 0x1) << 30;
    BlankStartHeader |= (SR & 0x1) << 31;
    BlankStartHeader |= (FillCount & 0xFF) << 8;

    // Calculate ECC for Blank Start Header
    uint8_t BlankStartECC = calculateECC(BlankStartHeader);
    BlankStartHeader |= (BlankStartECC & 0xFF);

    return BlankStartHeader;
}

void generate_BS_packet(uint32_t tunHeader, uint32_t blankStartHeader, const char* filename)
{
    uint32_t tunHeader_t;
    uint32_t blankStartHeader_t;

    // Allocate memory for headers and payload
    uint8_t tunHeaderArr[4];
    uint8_t blankStartHeaderArr[4];
    size_t payloadLength = 12;
    uint8_t payload[payloadLength];

    tunHeaderArr[0] = (tunHeader >> 24) & 0xFF;
    tunHeaderArr[1] = (tunHeader >> 16) & 0xFF;
    tunHeaderArr[2] = (tunHeader >> 8) & 0xFF;
    tunHeaderArr[3] = tunHeader & 0xFF;

    blankStartHeaderArr[0] = (blankStartHeader >> 24) & 0xFF;
    blankStartHeaderArr[1] = (blankStartHeader >> 16) & 0xFF;
    blankStartHeaderArr[2] = (blankStartHeader >> 8) & 0xFF;
    blankStartHeaderArr[3] = blankStartHeader & 0xFF;
    
    // Generate the payload sequentially
    generatePayload(payload, payloadLength);


    FILE *file = fopen(filename, "w");
    if(file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    // Output the Tunneled Packet
    for (size_t i = 0; i < 4; i++) {
        fprintf(file, "%02X ", tunHeaderArr[i]);
    }
    for (size_t i = 0; i < 4; i++) {
        fprintf(file, "%02X ", blankStartHeaderArr[i]);
    }
    for (size_t i = 0; i < payloadLength; i++) {
        fprintf(file, "%02X ", payload[i]);
    }

    fclose(file);
}

void BSP_GEN(const char* SRString, const char* CPString, const char* FillCountString, const char* filename)
{
    uint8_t SR = (uint8_t) strtol(SRString, NULL, 10);
    uint8_t CP = (uint8_t) strtol(CPString, NULL, 10);
    uint32_t FillCount = (uint32_t) strtol(FillCountString, NULL, 10);

    uint32_t tunHeader = generate_tunneled_BS_packet_header();
    uint32_t blankStartHeader = generate_BS_packet_header(SR, CP, FillCount);

    generate_BS_packet(tunHeader, blankStartHeader, filename);
}

// int main()
// {
//     BSP_GEN("1", "0", "100", "results/BSP.txt");
//     return 0;
// }