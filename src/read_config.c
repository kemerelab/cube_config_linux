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
// Description  : main function for reading the configuration information
//                from an sd card
// CL arguments : device file name
//                output file containing current configuration on device,
//                optional
// Returns     :  int - 0 if success, 1 if usage screen was displayed, 
//                negative value otherwise
//////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    char device_fname[MAX_FNAME_LENGTH];
    char output_fname[MAX_FNAME_LENGTH];
    int readDiskRes, deviceInfoRes, fileAccessRes;
    int i, j, count;
    uint8_t buff[BUFFER_LENGTH];
    DeviceInfoType deviceInfo;
    FilePermissionType permission;
    FILE *output_fp;
    
    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    fprintf(stdout, "\n*** read_config 1.0 ***\n");

    if ( 1 == argc) {
        fprintf(stdout, "\nUsage: read_config <device file> <output file>\n");
        fprintf(stdout, "<output file> is optional\n");
        fprintf(stdout, "Example: `read_config /dev/sdb config_file.cfg`\n");
        return 1;
    }
    else if ( 1 < argc ) {
        if ( 3 < argc) {
            fprintf(stdout, "\nYou specified %d arguments when read_config "
                    "only uses up to 2.\nIgnoring extra arguments\n", argc - 1);
        }

        // check file name length
        strncpy(device_fname, argv[1], MAX_FNAME_LENGTH);
        if ( '\0' != device_fname[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum device file name length exceeded.\n");
            return -1;
        }

        // check file existence and permissions
        permission = READ_ACCESS;
        fileAccessRes = DISKIO_iCheckFileAccess(device_fname, permission);
        if ( fileAccessRes ) {
            fprintf(stderr, "\nError checking read permission of %s: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    device_fname,
                    fileAccessRes);
            return -2;
        }

        // check that device information can be determined
        // have gotten strange results when using uninitialized deviceInfo so 
        // initialize here. In general it never hurts to initialize anyway  
        memset(&deviceInfo, 0, sizeof(deviceInfo));
        deviceInfoRes = DISKIO_iGetDeviceInfo(device_fname, &deviceInfo);
        if ( deviceInfoRes ) {
            fprintf(stderr, "\nError checking device info: return value"
                    " of DISKIO_iGetDeviceInfo() is %d\n",
                    deviceInfoRes);
            return -3;
        }

        // read configuration sector
        readDiskRes = DISKIO_iReadDisk(device_fname, buff, 0, 1, &deviceInfo);
        if ( readDiskRes ) {
            fprintf(stderr, "\nError reading configuration sector: "
                    "return value of DISKIO_iReadDisk() is %d\n",
                    readDiskRes);
            return -4;
        }

        // display configuration to console
        count = 0;
        for (i = 0; i < NUM_CHANNELS_PER_MODULE; i++) {
            fprintf(stdout, "Group %02d: ", i);
            if (buff[i]) {
                fprintf(stdout, "Card(s) ");
                for (j = 0; j < NUM_MODULES; j++) {
                    if ((buff[i] >> j) & 0x01) {
                        count++;
                        if ( 0 == j ) {
                            fprintf(stdout, "%d",j);
                        }
                        else {
                            fprintf(stdout, ",%d",j);
                        }
                    }
                }
                fprintf(stdout, " enabled\n");
            }
        }
        fprintf(stdout, "\n%d channels enabled, packet size = %d\n", count, 2*count + 14);

        // Write to output file the configuration that was on the card
        if ( 3 == argc ) {
            strncpy(output_fname, argv[2], MAX_FNAME_LENGTH);
            if ( '\0' != output_fname[MAX_FNAME_LENGTH-1] ) {
                fprintf(stderr, "\nMaximum output file name length exceeded.\n");
                return -5;
            }
            output_fp = fopen(output_fname, "w");
            if ( NULL == output_fp ) {
                fprintf(stderr, "\nError no %d opening output file %s: %s." 
                        " Please try again\n", 
                        errno, output_fname, strerror(errno));
                return -6;
            }
            for (i = 0; i < NUM_CHANNELS_PER_MODULE; i++) {
                for (j = (NUM_MODULES -1); j >= 0; j--) {
                    if ((buff[i] >> j) & 0x01) {
                            fprintf(output_fp, "1");
                        }
                        else {
                            fprintf(output_fp,"0");
                        }
                    }
                fprintf(output_fp,"\n");
            }
            fclose(output_fp);
        }

        fprintf(stdout, "Configuration read successfully!\n");
        return 0;
    }
}
  


