#include "headers/VDP.h"
#include "headers/utils.h"

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

uint32_t generate_VDP_TU_set_Header(uint32_t EOC, uint32_t TU_type, uint32_t L, uint32_t Fill_Count, uint32_t Video_Count)
{
    // Construct TU set Header
    uint32_t TU_set_header = ((calculateECC(HEC_INIT) & 0xFF) << 0) |
                             ((Video_Count & 0x3F) << 8) |
                             ((Fill_Count & 0x3FFF) << 14) |
                             ((L & 0x1) << 28) |
                             ((TU_type & 0x3) << 29) |
                             ((EOC & 0x1) << 31);
    return TU_set_header;
}

uint32_t *generate_VDP_TU_set_Headers(int argc, char *argv[])
{
    size_t num_TU_sets = (argc - 1) / 5;
    uint32_t *TU_set_headers = (uint32_t *)malloc(num_TU_sets * sizeof(uint32_t));
    for(int i=0 ; i<num_TU_sets ; i++)
    {
        uint32_t EOC = atoi(argv[1 + i*5]);
        uint32_t TU_type = atoi(argv[2 + i*5]);
        uint32_t L = atoi(argv[3 + i*5]);
        uint32_t Fill_Count = atoi(argv[4 + i*5]);
        uint32_t Video_Count = atoi(argv[5 + i*5]);

        TU_set_headers[i] = generate_VDP_TU_set_Header(EOC, TU_type, L, Fill_Count, Video_Count);
    }
    return TU_set_headers;
}

uint32_t generate_Tunneled_VDP_Header(uint32_t Length)
{
    uint8_t HopID = HOPID_DEFAULT;
    uint32_t USB4_header = (HEC_INIT) |
                           (Length << 8) |
                           (HopID << 16) |
                           (RESERVED << 23) |
                           (SUPP_ID << 27) |
                           (PDF_VIDEO_DATA << 28);
    uint8_t HEC = calculateHEC(USB4_header);
    USB4_header |= HEC;
    return USB4_header;
}

void generate_Tunneled_VD_Packet(uint32_t USB4_header, uint32_t *TU_set_headers, size_t num_TU_sets, FILE *file)
{
    uint8_t tunHeader[4];
    uint8_t **tuSetHeaders;
    uint8_t **payload;
    uint32_t *payloadLengths;
    
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
    }

    // Calculate the total number of bytes for the payload
    int totalVideoDataLength = calculateTotalVideoDataLength(num_TU_sets, (const unsigned char**)tuSetHeaders);
    int totalPacketLength = 4 + (num_TU_sets * 4) + totalVideoDataLength;  // 4 bytes for Tunneled Packet Header + TU headers + Video data
    tunHeader[2] = totalPacketLength - 4;  // Exclude the Tunneled Packet Header length

    char outputHex[totalPacketLength * 2 + 1];
    bytesToHexString(tunHeader, 4, file, 1);
    int bytes = 4;

    // Generate the payload for each TU set
    payload = (uint8_t **)malloc(num_TU_sets * sizeof(uint8_t *));
    payloadLengths = (uint32_t *)malloc(num_TU_sets * sizeof(uint32_t));
    for(int i=0 ; i<num_TU_sets ; i++)
    {
        // print TU set header
        payloadLengths[i] = extractVideoCount(tuSetHeaders[i]);

        bytesToHexString(tuSetHeaders[i], 4, file, 0);
        bytes += 4;
        for(int j=0 ; j<payloadLengths[i] ; j++)
        {
            unsigned char dummy[1] = { (unsigned char)j };
            bytesToHexString(dummy, 1, file, 0);
            bytes += 1;
        }
        
        // Allign the payload to 4 bytes
        for(int j=0 ; j<(((payloadLengths[i] + 3) & ~3) - payloadLengths[i]) ; j++)
        {
            unsigned char dummy[1] = { 0 };
            bytesToHexString(dummy, 1, file, 0);
            bytes += 1;
        }
        free(tuSetHeaders[i]);
    }

    free(tuSetHeaders);  
    free(payload);
    free(payloadLengths);
}


/**
 * Generate a Video Data Packet
 * @param argc Number of arguments
 * @param argv Strings: VDP <EOC_1> <TU_type_1> <L_1> <Fill_Count_1> <Video_Count_1> ... <EOC_n> <TU_type_n> <L_n> <Fill_Count_n> <Video_Count_n>
 * @param file File to save the packet to
 * 
*/
int VDP_GEN(int argc, char *argv[], FILE* file)
{
    if (argc == 1)
    {
        fprintf(stderr, "Usage: %s <EOC_1> <TU_type_1> <L_1> <Fill_Count_1> <Video_Count_1> ... <EOC_n> <TU_type_n> <L_n> <Fill_Count_n> <Video_Count_n>\n", argv[0]);
        return 1;
    }

    size_t num_TU_sets = 0;
    if ((argc - 1) % 5 != 0)
    {
        fprintf(stderr, "Error: Invalid number of arguments.\n");
        fprintf(stderr, "Usage: %s <EOC_1> <TU_type_1> <L_1> <Fill_Count_1> <Video_Count_1> ... <EOC_n> <TU_type_n> <L_n> <Fill_Count_n> <Video_Count_n>\n", argv[0]);

        return 1;
    }
    else num_TU_sets = (argc - 1) / 5;

    // ---------------------------------------------------
    // --------------- TU Set Header ---------------------
    // ---------------------------------------------------

    uint32_t *TU_set_headers = generate_VDP_TU_set_Headers(argc, argv);

    // ---------------------------------------------------
    // ----------- USB4 Tunneled Packet Header -----------
    // ---------------------------------------------------
    
    uint32_t Length = 0; // Calculated in the payload generation
    uint32_t USB4_header = generate_Tunneled_VDP_Header(Length);

    // ---------------------------------------------------
    // --------------- Generate Packet -------------------
    // ---------------------------------------------------
    generate_Tunneled_VD_Packet(USB4_header, TU_set_headers, num_TU_sets, file);

    return 0;
}