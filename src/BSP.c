# include "headers/BSP.h"
# include "headers/utils.h"

void get_BSP_payload_format(enum PAYLOAD_TYPE payloadFormat[], int lane)
{
    switch(lane)
    {
        case 1:
        {
            enum PAYLOAD_TYPE BSP_PAYLOAD_FORMAT_LANE_1[12] = {VBID, VBID, VBID, VBID, MVID, MVID, MVID, MVID, MAUD, MAUD, MAUD, MAUD};
            for(int i=0 ; i<12 ; i++)
                payloadFormat[i] = BSP_PAYLOAD_FORMAT_LANE_1[i];
            return;
        }
        case 2:
        {
            enum PAYLOAD_TYPE BSP_PAYLOAD_FORMAT_LANE_2[12] = {VBID, VBID, MVID, MVID, MAUD, MAUD, VBID, VBID, MVID, MVID, MAUD, MAUD};
            for(int i=0 ; i<12 ; i++)
                payloadFormat[i] = BSP_PAYLOAD_FORMAT_LANE_2[i];
            return;
        }
        case 4:
        {
            enum PAYLOAD_TYPE BSP_PAYLOAD_FORMAT_LANE_4[12] = {VBID, MVID, MAUD, VBID, MVID, MAUD, VBID, MVID, MAUD, VBID, MVID, MAUD};
            for(int i=0 ; i<12 ; i++)
                payloadFormat[i] = BSP_PAYLOAD_FORMAT_LANE_4[i];
            return;
        }
        default:
            payloadFormat = NULL;
            return; 
    }
}

// Generate a payload sequentially starting from 00
void generatePayload(uint8_t *payload, size_t payloadLength) {
    int lane = 1;

    enum PAYLOAD_TYPE payloadFormat[12];
    get_BSP_payload_format(payloadFormat, lane);
    if(payloadFormat == NULL) {
        printf("Invalid lane configuration.\n");
        return;
    }

    for (size_t i = 0; i < payloadLength; i++)
        payload[i] = payloadFormat[i] & 0xFF;

    // switch(lane)
    // {
    //     case 1:
    //         for (size_t i = 0; i < payloadLength; i++)
    //             payload[i] = BSP_PAYLOAD_FORMAT_LANE_1[i] & 0xFF; // Keep incrementing from 00
    //         break;
    //     case 2:
    //         for (size_t i = 0; i < payloadLength; i++)
    //             payload[i] = BSP_PAYLOAD_FORMAT_LANE_2[i] & 0xFF; // Keep incrementing from 00
    //         break;
    //     case 4:
    //         for (size_t i = 0; i < payloadLength; i++)
    //             payload[i] = BSP_PAYLOAD_FORMAT_LANE_4[i] & 0xFF; // Keep incrementing from 00
    //         break;
    //     default:
    //         printf("Invalid lane configuration.\n");
    // }
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

void generate_BS_packet(uint32_t tunHeader, uint32_t blankStartHeader, FILE *file)
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


    if(file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    bytesToHexString(tunHeaderArr, 4, file, 1);
    bytesToHexString(blankStartHeaderArr, 4, file, 0);
    bytesToHexString(payload, payloadLength, file, 0);
}

/**
 * 
 * Generate a Blank Start Packet
 * @param SRString SR as a string
 * @param CPString CP as a string
 * @param FillCountString Fill Count as a string
 * @param file File to save the packet to
 * 
*/
void BSP_GEN(const char* SRString, const char* CPString, const char* FillCountString, FILE* file)
{
    uint8_t SR = (uint8_t) strtol(SRString, NULL, 10);
    uint8_t CP = (uint8_t) strtol(CPString, NULL, 10);
    uint32_t FillCount = (uint32_t) strtol(FillCountString, NULL, 10);

    uint32_t tunHeader = generate_tunneled_BS_packet_header();
    uint32_t blankStartHeader = generate_BS_packet_header(SR, CP, FillCount);

    generate_BS_packet(tunHeader, blankStartHeader, file);
}

// int main()
// {
//     BSP_GEN("1", "0", "100", "results/BSP.txt");
//     return 0;
// }