#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include "diskio_linux.h"

#define BUFFER_LENGTH 65536
#define MAX_FNAME_LENGTH 1000
#define NUM_PACKETS 1       // number of packets to read at a time
#define PROGRESS_PERCENT 5
#define SAMPLING_RATE 30000   // samples/sec
#define START_BYTE_IND 0
#define FLAG_BYTE_IND 2
#define TIMESTAMP_START_IND 10
#define START_BYTE_VAL 0x55
#define RF_VALID_VAL 0x1

//////////////////////////////////////////////////////////////////////////
// Function     : main()
// Description  : main function for extracting data recorded on disk
// CL arguments : device file name
//                file name for extracted data
// Returns      : int - 0 if success, 1 if usage screen was displayed, 
//                negative value otherwise
//////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    char deviceFile[MAX_FNAME_LENGTH];
    char outputFile[MAX_FNAME_LENGTH];
    int i, readAccessRes, deviceInfoRes, readPacketRes, readDiskRes;
    int rfSyncCt;
    uint8_t buff[BUFFER_LENGTH];
    uint32_t shift, psize;
    uint64_t bytesWritten;
    uint64_t lastPacket, maxNumPackets, nPacketsProgress;
    uint64_t nDroppedPackets;
    uint64_t nDroppedPacketsCounted, packetIndex;
    FilePermissionType permission;
    DeviceInfoType deviceInfo;
    FILE *fpDevice, *fpOutput;

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    fprintf(stdout, "\n*** sd_card_extract 1.0 ***\n");

    if ( 1 == argc) {
        fprintf(stdout, "\nUsage: sd_card_extract [DEVICE_FILENAME]" 
                " [EXTRACTED_DATA_FILENAME]\n");
        fprintf(stdout, "Example: `sd_card_extract /dev/sdb extracted_data.dat`\n");
        return 1;
    }
    else if ( 2 == argc) {
        fprintf(stderr, "\nNot enough arguments!\n");
        return -1;
    }
    else if ( 2 < argc ) {
        if ( 3 < argc) {
            fprintf(stderr, "\nYou specified %d arguments when sd_card_extract "
                    "only uses 2. Ignoring extra arguments\n", argc - 1);
        }

        // check file name lengths
        strncpy(deviceFile, argv[1], MAX_FNAME_LENGTH);
        strncpy(outputFile, argv[2], MAX_FNAME_LENGTH);
        if ( '\0' != deviceFile[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum device file name length exceeded.\n");
            return -2;
        }
        if ( '\0' != outputFile[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum output file name length exceeded.\n");
            return -3;
        }

        // check file permissions
        permission = READ_ACCESS;
        readAccessRes = DISKIO_iCheckFileAccess(deviceFile, permission);
        if ( 0 != readAccessRes ) {
            fprintf(stderr, "\nError checking read permission: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    readAccessRes);
            return -4;
        }

        // check that device information can be determined
        // have gotten strange results when using uninitialized deviceInfo so 
        // initialize here. In general it never hurts to initialize anyway  
        memset(&deviceInfo, 0, sizeof(deviceInfo));
        deviceInfoRes = DISKIO_iGetDeviceInfo(deviceFile, &deviceInfo);
        if ( 0 != deviceInfoRes ) {
            fprintf(stderr, "\nError checking device info: return value"
                    " of DISKIO_iGetDevice() is %d\n",
                    deviceInfoRes);
            return -5;
        }        

        // read second and third sectors (first sector is configuration info)
        readDiskRes = DISKIO_iReadDisk(deviceFile, buff, 1, 2, &deviceInfo);
        if ( 0 != readDiskRes ) {
            fprintf(stderr, "\nError reading from device: "
                    "return value of DISKIO_iReadDisk() is %d\n", readDiskRes);
            return -6;
        }
        // check first packet header
        if (buff[START_BYTE_IND] != START_BYTE_VAL) {
            fprintf(stdout, "\nNo start packet found!\n");
            return -7;
        }
        // search for the next packet header, assuming we have a 10-byte header 
        // file and thus the second packet timestamp will begin at the 11th byte
        while (((buff[i] != START_BYTE_VAL) ||
                (buff[i + TIMESTAMP_START_IND] != 0x01) ||
                (buff[i + TIMESTAMP_START_IND + 1] != 0x00) ||
                (buff[i + TIMESTAMP_START_IND + 2] != 0x00) ||
                (buff[i + TIMESTAMP_START_IND + 3] != 0x00)) &&
                (i++ < deviceInfo.sectorSize));
        if (i >= deviceInfo.sectorSize) {
            fprintf(stderr, "\nCan't find the second packet start!\n");
            return -6;
        } 
        else {
            psize = i;
        }

        fpDevice = fopen(deviceFile, "r");
        if ( NULL == fpDevice ) {
            fprintf(stderr, "Could not open %s to extract data!\n", deviceFile);
            return -8;
        }

        fprintf(stdout, "Packet size: %u bytes/packet\n", (unsigned)psize);

        // Maximum packets is device size - size of one sector 
        maxNumPackets = ( (deviceInfo.sectorCount -1) * deviceInfo.sectorSize)/psize;
        fprintf(stdout, "Maximum packets on the disk = %llu (%.2f minutes)\n",
                (long long unsigned)maxNumPackets,
                (double)maxNumPackets/SAMPLING_RATE/60.0 );

        lastPacket = 1;
        shift = 0;
        // determine the most significant bit of the maximum packet index
        while (lastPacket < maxNumPackets) {
            shift++;
            lastPacket = lastPacket<<1;
        }
        lastPacket = 0;
        for (i = shift; i >= 0; i--) {
            lastPacket |= (1<<i);
            if (lastPacket >= maxNumPackets) {
                lastPacket &= ~(1<<i);
                continue;
            }
            readPacketRes = DISKIO_iReadPacket(fpDevice, buff, 
                                               lastPacket, psize, 1,
                                               &deviceInfo);
            if ( readPacketRes ) {
                fprintf(stderr, "\nError reading packet %llu: return value"
                        " of DISKIO_iReadPacket() is %d\n", 
                        (long long unsigned)lastPacket, 
                        readPacketRes);
                return -9;
            } 
            else if (buff[START_BYTE_IND] != START_BYTE_VAL) {
                // packet that was just read is greater than the actual last
                // packet since the starting byte of the packet is not 
                // START_BYTE_VAL. so set bit i to 0, set bits NOT i to 1, 
                // and bit wise AND to keep all NOT i bits in lastPacket the 
                // same as before. Next iteration of for loop will check 
                // bit i-1 to see whether there is a valid packet.
                lastPacket &= ~(1<<i);
            }
        }
        if (lastPacket < (maxNumPackets-1)) {
            // Disk not full, don't use the last recorded packet since it might not be complete
            lastPacket--;
        }

        fprintf(stdout, "Packets recorded on the disk = %lu (%.2f minutes)\n",
                (long unsigned)(lastPacket + 1),
                (double)(lastPacket+1)/SAMPLING_RATE/60.0 );
        readPacketRes = DISKIO_iReadPacket(fpDevice, buff, lastPacket, psize, 1, &deviceInfo);
        if (readPacketRes) {
            fprintf(stderr, "\nError reading last packet recorded on disk: return value"
                    " of DISKIO_iReadPacket() is %d\n", readPacketRes);
            return -10;
        }
        
        // if there are dropped packets, the timestamp of the last packet will be greater
        // than the number of packets recorded on disk
        nDroppedPackets = (buff[TIMESTAMP_START_IND + 3] << 24 |
                           buff[TIMESTAMP_START_IND + 2] << 16 |
                           buff[TIMESTAMP_START_IND + 1] <<  8 |
                           buff[TIMESTAMP_START_IND])
                           - lastPacket;
        if ( nDroppedPackets ) {
            fprintf(stdout, "Dropped packets = %llu (%.2f msec = %.2f sec)\n", 
                    (long long unsigned)nDroppedPackets, 
                    (float)nDroppedPackets / SAMPLING_RATE * 1000,
                    (float)nDroppedPackets / SAMPLING_RATE);
        }
        else { 
            fprintf(stdout, "No dropped packets\n");
        }

        fprintf(stdout, "Extracting the data:\n");
        fpOutput = fopen(outputFile, "w");
        if ( NULL == fpOutput ) {
            fprintf(stderr, "Error opening file %s to extract data to!\n", outputFile);
            return -11;
        }
        packetIndex = 0;

        // will be used to display how frequently progress occurs 
        nPacketsProgress = floor(0.01 * lastPacket * PROGRESS_PERCENT);
        
        // read NUM_PACKETS at a time
        while ( (packetIndex + NUM_PACKETS) < lastPacket ) {
            if ( packetIndex % nPacketsProgress == 0 ) {
                fprintf(stdout, "%4.1f%% completed\n", (float)packetIndex / (float)lastPacket * 100);
            }

            readPacketRes = DISKIO_iReadPacket(fpDevice, buff, packetIndex, 
                                              psize, NUM_PACKETS, &deviceInfo);
            if ( readPacketRes ) {
                fprintf(stderr, "Error reading packets %llu to %llu!\n",
                        (long long unsigned)packetIndex,
                        (long long unsigned)(packetIndex + NUM_PACKETS - 1) );
                return -12;
            }

            // check that value of start byte is as expected for sd recording
            if ( buff[START_BYTE_IND] == START_BYTE_VAL ) {
                if ( buff[FLAG_BYTE_IND] == RF_VALID_VAL ) {
                    ++rfSyncCt;
                }
                bytesWritten = (uint64_t)fwrite(buff, 1, psize*NUM_PACKETS, fpOutput);
                if ( (psize*NUM_PACKETS) != bytesWritten ) {
                fprintf(stderr, "Error: %llu bytes requested to write but %llu"
                        " bytes actually written when writing packets %llu to %llu\n",
                        (long long unsigned)(psize*NUM_PACKETS),
                        (long long unsigned)bytesWritten,
                        (long long unsigned)packetIndex,
                        (long long unsigned)(packetIndex + NUM_PACKETS - 1) );
                return -13;
                }
            }
            else {
                fprintf(stderr, "Bad packet found. Packet index: %llu, \
                        byte[%u] value: %2x. Not saving bad packet to output file\n",
                        (long long unsigned)packetIndex,
                        (unsigned)START_BYTE_IND,
                        (unsigned)buff[START_BYTE_IND] );
            }
            packetIndex += NUM_PACKETS;

        }
        
        // read the last block of packets
        readPacketRes = DISKIO_iReadPacket(fpDevice, buff, packetIndex, psize, 
                                           lastPacket - packetIndex + 1, &deviceInfo);
        if ( readPacketRes ) {
            fprintf(stderr, "Error reading packets %llu to %llu!\n", 
                    (long long unsigned)packetIndex,
                    (long long unsigned)lastPacket );
            return -14;
        }
        bytesWritten = (uint64_t)fwrite(buff, 1, psize*(lastPacket - packetIndex + 1), fpOutput);
        if ( psize*(lastPacket - packetIndex + 1) != bytesWritten ) {
            fprintf(stderr, "Error %llu bytes requested to write but %llu"
                    " bytes actually written when writing packets %llu to %llu\n", 
                    (long long unsigned)(psize*(lastPacket - packetIndex +1)), 
                    (long long unsigned)bytesWritten,
                    (long long unsigned)packetIndex,
                    (long long unsigned)lastPacket );
            return -15;
        }
        if ( fclose(fpDevice) ) {
            fprintf(stderr, "Error closing %s after extracting data\n", deviceFile);
            return -16;
        }
        if ( fclose(fpOutput) ) {
            fprintf(stderr, "Error closing %s after extracting data\n", outputFile);
            return -17;
        }

        // RF sync values found
        if ( rfSyncCt ) {
            fprintf(stdout, "\nFound %d RF sync values\n", rfSyncCt);
        }
        else {
            fprintf(stderr, "\nError: Found 0 RF sync values!\n");
        }

        fprintf(stdout, "Done!\n");
        return 0; 
    }
}
