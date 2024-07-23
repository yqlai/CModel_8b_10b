#include "headers/VDP.h"


// Utility function to convert a string of hex digits into a byte array
void hexStringToBytes(const char* hexString, unsigned char* byteArray) {
    while (*hexString) {
        sscanf(hexString, "%2hhx", byteArray++);
        hexString += 2;
    }
}

// Utility function to convert byte array back to hex string
void bytesToHexString(const unsigned char* byteArray, int length, char* hexString) {
    for (int i = 0; i < length; i++) {
        sprintf(hexString + (i * 2), "%02X", byteArray[i]);
    }
}

// Parse and extract Video Count from TU set header
int extractVideoCount(const unsigned char* tuSetHeader) {
    int videoCount = (tuSetHeader[2] & 0x3F);  // Extract bits [13:8]
    if (videoCount == 0) videoCount = 64;
    return videoCount;
}

// Parse and calculate the total video data length
int calculateTotalVideoDataLength(int totalTuSets, const unsigned char** tuSetHeaders) {
    int totalVideoDataLength = 0;
    for (int i = 0; i < totalTuSets; i++) {
        totalVideoDataLength += extractVideoCount(tuSetHeaders[i]);
    }
    return totalVideoDataLength;
}

// Helper function to calculate HEC
uint8_t calculate_HEC(uint32_t header)
{
    uint8_t hec = HEC_INIT;
    for (int i = 23; i >= 0; --i)
    {
        uint8_t bit = ((header >> i) & 1) ^ ((hec >> 7) & 1);
        hec <<= 1;
        if (bit)
        {
            hec ^= 0x07;
        }
    }
    return hec ^ HEC_XOR_OUT;
}

// Helper function to calculate ECC
uint8_t calculate_ECC(uint32_t header)
{
    uint8_t ecc = ECC_INIT;
    for (int i = 23; i >= 0; --i)
    {
        uint8_t bit = ((header >> i) & 1) ^ ((ecc >> 7) & 1);
        ecc <<= 1;
        if (bit)
        {
            ecc ^= 0x07;
        }
    }
    return ecc;
}

// Fill the payload with incremental values (00, 01, 02, etc.)
void fillPayload(unsigned char* payload, int length) {
    for (int i = 0; i < length; i++) {
        payload[i] = i & 0xFF;
    }
}

uint32_t generate_TU_set_Header(uint32_t EOC, uint32_t TU_type, uint32_t L, uint32_t Fill_Count, uint32_t Video_Count)
{
    // Construct TU set Header
    uint32_t TU_set_header = (calculate_ECC(HEC_INIT) << 0) |
                             (Video_Count << 8) |
                             (Fill_Count << 14) |
                             (L << 28) |
                             (TU_type << 29) |
                             (EOC << 31);
    return TU_set_header;
}

uint32_t *generate_TU_set_Headers(int argc, char *argv[])
{
    size_t num_TU_sets = (argc - 2) / 5;
    uint32_t *TU_set_headers = (uint32_t *)malloc(num_TU_sets * sizeof(uint32_t));
    for(int i=0 ; i<num_TU_sets ; i++)
    {
        uint32_t EOC = atoi(argv[1 + i*5]);
        uint32_t TU_type = atoi(argv[2 + i*5]);
        uint32_t L = atoi(argv[3 + i*5]);
        uint32_t Fill_Count = atoi(argv[4 + i*5]);
        uint32_t Video_Count = atoi(argv[5 + i*5]);

        TU_set_headers[i] = generate_TU_set_Header(EOC, TU_type, L, Fill_Count, Video_Count);
    }
    return TU_set_headers;
}

uint32_t generate_Tunneled_VDP_Header(uint32_t Length, uint8_t HopID)
{
    uint32_t USB4_header = (HEC_INIT) |
                           (Length << 8) |
                           (HopID << 16) |
                           (RESERVED << 23) |
                           (SUPP_ID << 27) |
                           (PDF_VIDEO_DATA << 28);
    uint8_t HEC = calculate_HEC(USB4_header);
    USB4_header |= HEC;
    return USB4_header;
}

void generate_Tunneled_VD_Packet(uint32_t USB4_header, uint32_t *TU_set_headers, size_t num_TU_sets)
{
    uint8_t tunHeader[4];
    uint8_t **tuSetHeaders;
    
    tunHeader[0] = (USB4_header >> 24) & 0xFF;
    tunHeader[1] = (USB4_header >> 16) & 0xFF;
    tunHeader[2] = (USB4_header >> 8) & 0xFF;
    tunHeader[3] = USB4_header & 0xFF;
    
    tuSetHeaders = (uint8_t **)malloc(num_TU_sets * sizeof(uint8_t *));
    for(int i=0 ; i<num_TU_sets ; i++)
    {
        tuSetHeaders[i] = (uint8_t *)malloc(4 * sizeof(uint8_t));
        tuSetHeaders[i][0] = (TU_set_headers[i] >> 24) & 0xFF;
        tuSetHeaders[i][1] = (TU_set_headers[i] >> 16) & 0xFF;
        tuSetHeaders[i][2] = (TU_set_headers[i] >> 8) & 0xFF;
        tuSetHeaders[i][3] = TU_set_headers[i] & 0xFF;

        // Calculate the total number of bytes for the payload
        int totalVideoDataLength = calculateTotalVideoDataLength(num_TU_sets, (const unsigned char**)tuSetHeaders);
        int totalPacketLength = 4 + (num_TU_sets * 4) + totalVideoDataLength;  // 4 bytes for Tunneled Packet Header + TU headers + Video data

        //Update the Length field in the Tunneled Packet Header
        tunHeader[2] = totalPacketLength - 4;  // Exclude the Tunneled Packet Header length

        // Generate the payload
        unsigned char payload[totalVideoDataLength];
        fillPayload(payload, totalVideoDataLength);

        // Output the final packet in hexadecimal format
        char outputHex[totalPacketLength * 2 + 1];
        bytesToHexString(tunHeader, 4, outputHex);
        for (int i = 0; i < num_TU_sets; i++) {
            bytesToHexString(tuSetHeaders[i], 4, outputHex + (4 * 2) + (i * 4 * 2));
            free(tuSetHeaders[i]);  // Free allocated memory for TU set headers
    }
    bytesToHexString(payload, totalVideoDataLength, outputHex + (4 * 2) + (num_TU_sets * 4 * 2));

    // Print the final packet
    printf("%s\n", outputHex);
    }
}

int VDP_GEN(int argc, char *argv[])
{
    if (argc == 0)
    {
        fprintf(stderr, "Usage: %s <HopID> <EOC_1> <TU_type_1> <L_1> <Fill_Count_1> <Video_Count_1> ... <EOC_n> <TU_type_n> <L_n> <Fill_Count_n> <Video_Count_n>\n", argv[0]);
        return 1;
    }

    size_t num_TU_sets = 0;
    if ((argc - 2) % 5 != 0)
    {
        fprintf(stderr, "Error: Invalid number of arguments.\n");
        fprintf(stderr, "Usage: %s <HopID> <EOC_1> <TU_type_1> <L_1> <Fill_Count_1> <Video_Count_1> ... <EOC_n> <TU_type_n> <L_n> <Fill_Count_n> <Video_Count_n>\n", argv[0]);

        return 1;
    }
    else num_TU_sets = (argc - 2) / 5;

    // ---------------------------------------------------
    // --------------- TU Set Header ---------------------
    // ---------------------------------------------------

    uint32_t *TU_set_headers = generate_TU_set_Headers(argc, argv);

    // ---------------------------------------------------
    // ----------- USB4 Tunneled Packet Header -----------
    // ---------------------------------------------------
    
    uint32_t Length = 0; // Calculated in the payload generation
    uint8_t HopID = atoi(argv[1]);
    uint32_t USB4_header = generate_Tunneled_VDP_Header(Length, HopID);

    return 0;
}

// int main(int argc, char *argv[])
// {
//     VDP_GEN(argc, argv);
//     return 0;
// }
