#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "diskio_linux.h"

#define MAX_FNAME_LENGTH 1000
#define BUFFER_LENGTH 32768
#define DEFAULT_SECTOR_SIZE 512

//////////////////////////////////////////////////////////////////////////
// Function     : main()
// Description  : main function for enabling an SD card for recording
// CL arguments : device file name
// Returns      : int - 0 if success, 1 if usage screen was displayed, 
//                negative value otherwise
//////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    char deviceFile[MAX_FNAME_LENGTH];
    char outputFile[MAX_FNAME_LENGTH];
    int readDiskRes, writeDiskRes;
    int readAccessRes, writeAccessRes, deviceInfoRes;
    int i, j;
    uint8_t buff[BUFFER_LENGTH];
    FILE *output_fp;
    DeviceInfoType deviceInfo;
    FilePermissionType permission;
 
    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    fprintf(stdout, "\n*** card_enable 1.0 ***\n");

    if ( 1 == argc) {
        fprintf(stdout, "\nUsage: card_enable [DEVICE_FILENAME]\n");
        fprintf(stdout, "Example: `card_enable /dev/sdb`\n");
        return 1;
    }
    else if ( 1 < argc ) {
        if ( 2 < argc) {
            fprintf(stdout, "\nYou specified %d arguments when card_enable "
                    "only uses 1.\nIgnoring extra arguments\n", argc - 1);
        }

        // check file name length
        strncpy(deviceFile, argv[1], MAX_FNAME_LENGTH);
        if ( '\0' != deviceFile[MAX_FNAME_LENGTH-1] ) {
            fprintf(stderr, "\nMaximum device file name length exceeded.\n");
            return -1;
        }

        // check file permissions
        permission = READ_ACCESS;
        readAccessRes = DISKIO_iCheckFileAccess(deviceFile, permission);
        if ( readAccessRes ) {
            fprintf(stderr, "\nError checking read permission: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    readAccessRes);
            return -2;
        }
        permission = WRITE_ACCESS;
        writeAccessRes = DISKIO_iCheckFileAccess(deviceFile, permission);
        if ( writeAccessRes ) {
            fprintf(stderr, "\nError checking read permission: "
                    "return value of DISKIO_iCheckFileAccess() is %d\n",
                    writeAccessRes);
            return -3;
        }

        // check that device information can be determined
        // have gotten strange results when using uninitialized deviceInfo so 
        // initialize here. In general it never hurts to initialize anyway  
        memset(&deviceInfo, 0, sizeof(deviceInfo));
        deviceInfoRes = DISKIO_iGetDeviceInfo(deviceFile, &deviceInfo);
        if ( deviceInfoRes ) {
            fprintf(stderr, "\nError checking device info: return value"
                    " of DISKIO_iGetDeviceInfo() is %d\n",
                    deviceInfoRes);
            return -4;
        }

        // fill buffer with values we will write to the disk and check if data written 
        // without errors
        for (i = 0; i < deviceInfo.sectorSize; i++) {
            buff[i] = 0xaa;
        }
        writeDiskRes = DISKIO_iWriteDisk(deviceFile, buff, 1, 1, &deviceInfo);
        if ( writeDiskRes ) {
        	fprintf(stderr, "\nError enabling card for recording: "
        		    "return value of DISKIO_iWriteDisk() is %d\n",
        		    writeDiskRes );
        	return -5;
        }

        // confirm that card was enabled for recording successfully
        fprintf(stdout, "\nConfirming that card was enabled successfully\n");
        readAccessRes = DISKIO_iReadDisk(deviceFile, buff, 1, 1, &deviceInfo);
        if ( readAccessRes ) {
        	fprintf(stderr, "\nError confirming that card was enabled successfully: "
        		    "return value of DISKIO_iReadDisk() is %d\n", 
        		    readAccessRes);
            return -6;
        }
        else {
        	if ( 0xaa != buff[0] ) {
        		fprintf(stderr, "\nError: Card not enabled successfully for"
        			    " recording!\n");
        		return -7;
        	}
        	else {
        		fprintf(stdout, "\nCard enabled successfully for recording!\n");
        	}
        }

        return 0;
    }
}
  


