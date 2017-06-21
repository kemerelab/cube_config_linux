#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "diskio_linux.h"

#define MAX_FNAME_LENGTH 1000
#define BUFFER_LENGTH 32768
#define NUM_CHANNELS_PER_MODULE 32
#define NUM_MODULES 8

//////////////////////////////////////////////////////////////////////////
// Function     : main()
// Description  : main function for writing configuration information
//                to an sd card
// CL arguments : device file name
//                config file name
// Returns      : int - 0 if success, 1 if usage screen was displayed, 
//                negative value otherwise
//////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    char configFile[MAX_FNAME_LENGTH];
    char deviceFile[MAX_FNAME_LENGTH];
    char str[100];
    int i, j, count;
    int readDiskRes, writeDiskRes, deviceInfoRes; 
    int readAccessRes, writeAccessRes;
    uint8_t buff[BUFFER_LENGTH];
    uint8_t mask;
    DeviceInfoType deviceInfo;
    FilePermissionType permission;
    FILE *fpConfigFile;
  
    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    fprintf(stdout, "\n*** write_config 1.0 ***\n");


    if ( 1 == argc) {
        fprintf(stdout, "\nUsage: write_config [DEVICE_FILENAME] [CONFIG_FILENAME\n");
        fprintf(stdout, "Example: `write_config /dev/sdb config_file.cfg`\n");
        return 1;
    }
    else if ( 2 == argc) {
        fprintf(stderr, "\nNot enough arguments!\n");
        return -1;
    }
    else if ( 2 < argc ) {
        if ( 3 < argc ) {
            fprintf(stderr, "\nYou specified %d arguments when write_config "
                    "uses only 2. Ignoring extra arguments\n", argc - 1);
        }

        // check file name lengths
        strncpy(deviceFile, argv[1], MAX_FNAME_LENGTH);
        strncpy(configFile, argv[2], MAX_FNAME_LENGTH);
        if ( '\0' != deviceFile[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum device file name length exceeded.\n");
            return -2;
        }
        if ( '\0' != configFile[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum config file name length exceeded.\n");
            return -3;
        }

        // check file permissions
        // needed to confirm that new configuration was actually written correctly
        permission = READ_ACCESS;
        readAccessRes = DISKIO_iCheckFileAccess(deviceFile, permission);
        if ( readAccessRes ) {
            fprintf(stderr, "\nError checking read permission of %s: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    deviceFile, readAccessRes);
            return -4;
        }
        permission = WRITE_ACCESS;
        writeAccessRes = DISKIO_iCheckFileAccess(deviceFile, permission);
        if ( writeAccessRes ) {
            fprintf(stderr, "\nError checking write permission of %s: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    deviceFile, writeAccessRes);
            return -5;
        }        

        // check that device information can be determined
        // have gotten strange results when using uninitialized deviceInfo so 
        // initialize here. In general it never hurts to initialize anyway  
        memset(&deviceInfo, 0, sizeof(deviceInfo));
        deviceInfoRes = DISKIO_iGetDeviceInfo(deviceFile, &deviceInfo);
        if ( deviceInfoRes ) {
            fprintf(stderr, "\nError checking device info: "
                    "return value of DISKIO_iGetDeviceInfo() is %d\n",
                    deviceInfoRes);
            return -6;
        }

        fpConfigFile = fopen(configFile, "r");
        if ( NULL == fpConfigFile ) { 
            fprintf(stderr, "Error no %d opening config file %s: %s\n", 
                    errno, configFile, strerror(errno) );
            return -7;
        }
        for (i = 0; i < deviceInfo.sectorSize; i++) {
            buff[i] = 0;
        } 
        for (i = 0; i < NUM_CHANNELS_PER_MODULE; i++) {
            fscanf (fpConfigFile, "%s", str);
            mask = 0;
            if (strlen(str) == NUM_MODULES) {
                for (j = 0; j < NUM_MODULES; j++) {
                    mask = mask << 1;
                    if (str[j] == '1')
                        mask |= 0x01;
                }
            }
            buff[i] = mask;
        }

        fprintf(stdout, "\nOverwriting card configuration data with data from file %s!\n", 
                configFile);
        // write configuration sector
        writeDiskRes = DISKIO_iWriteDisk(deviceFile, buff, 0, 1, &deviceInfo);
        if ( readDiskRes ) {
            fprintf(stderr, "\nError writing new configuration: "
                    "return value of DISKIO_iWriteDisk() is %d\n",
                    readDiskRes);
            return -8;
        }

        if ( fclose(fpConfigFile) ) {
            fprintf(stderr, "\nError no %d closing config file: %s"
                    " Recommend trying again to be safe.\n", 
                    errno, strerror(errno) );
            return -9;
        }

        // confirm that new configuration was written correctly
        readDiskRes = DISKIO_iReadDisk(deviceFile, buff, 0, 1, &deviceInfo);
        if ( readDiskRes ) {
            fprintf(stderr, "\nError confirming that new configuration was"
                    " written correctly: return value of DISKIO_iReadDisk() is %d\n",
                    readDiskRes);
            return -10;
        }

        fprintf(stdout, "\nNew configuration:\n");
        count = 0;
        for (i = 0; i < NUM_CHANNELS_PER_MODULE; i++) {
            fprintf(stdout, "Group %02d: ", i);
            if (buff[i]) {
                fprintf(stdout, "Card(s) ");
                for (j = 0; j < NUM_MODULES; j++) {
                    if ((buff[i] >> j) & 0x01) {
                        count++;
                        if (j == 0) {
                            fprintf(stdout, "%d", j);
                        }
                        else {
                            fprintf(stdout, ",%d", j);
                        }    
                    }
                }
                fprintf(stdout, " enabled\n");
            }
        }
        fprintf(stdout, "\n%d channels enabled, packet size = %d\n", count, 2*count + 14);

        fprintf(stdout, "New configuration written successfully!\n");
        return 0;
    } 
}
  


