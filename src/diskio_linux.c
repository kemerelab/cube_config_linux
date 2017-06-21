#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <linux/fs.h> // BLKSSZGET, BLKGETSIZE64 
#include <fcntl.h>    // O_RDONLY, O_NONBLOCK
#include "diskio_linux.h"

//////////////////////////////////////////////////////////////////////////
// Function    : DISKIO_iCheckFileAccess()
// Description : Checks a file's existence and permissions
// Parameters  : char *filename - the name of the file to be checked
//               FilePermission permission - type of permission to check
// Returns     : int - 0 if success, negative value otherwise
//////////////////////////////////////////////////////////////////////////
int DISKIO_iCheckFileAccess(char *filename, FilePermissionType permission) {

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    // check that file actually exists
    if ( -1 == access(filename, F_OK) ) {
        fprintf(stderr, "\nError: %s does not exist!\n", filename);
        return -1;
    }

    // check requested permission
    switch(permission) {
        case READ_ACCESS:
            if ( -1 == access(filename, R_OK) ) {
                fprintf(stderr, "\nYou do not have read permission for file %s! "
                        "Try using sudo\n", filename);
                return -1;
            }
                break;
        case WRITE_ACCESS:
            if ( -1 == access(filename, W_OK) ) {
                fprintf(stderr, "\nYou do not have write permission for file %s! "
                        "Try using sudo\n", filename);
                return -2;
            }
                break;
        case EXECUTE_ACCESS:
            if ( -1 == access(filename, X_OK) ) {
                fprintf(stderr, "\nYou do not have execute permission for file %s! "
                        "Try using sudo\n", filename);
                return -3;
            }
            break;
        default:
            fprintf(stderr, "\nInvalid parameter!\n");
            return -4;
            break;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Function    : DISKIO_iGetDeviceInfo()
// Description : Gets block information about a device e.g. sector size
// Parameters  : char *filename - The name of the device file
//               DeviceInfoType deviceInfoObj - Object that will hold the 
//                                              device information 
// Returns     : int - 0 if success, negative value otherwise
//////////////////////////////////////////////////////////////////////////
int DISKIO_iGetDeviceInfo(char *filename, DeviceInfoType *deviceInfoObj) {

    int device_fd;

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    fprintf(stdout, "\nGetting info from device %s ...\n", filename);
    device_fd = open(filename, O_RDONLY|O_NONBLOCK);
    if ( -1 == device_fd ) {
        fprintf(stderr, "\nError %d opening device: %s \n",
                errno, strerror(errno) );
        return -3;
    }
        
    if ( -1 == ioctl(device_fd, BLKSSZGET, &(deviceInfoObj->sectorSize)) ) {
        fprintf(stderr, "\nError getting sector size!\n");
        fprintf(stderr, "Error %d: %s \n", errno, strerror(errno));
        return -1;
    }
    else if ( -1 == ioctl(device_fd, BLKGETSIZE64, &(deviceInfoObj->deviceSize)) ) {
        fprintf(stderr, "\nError getting device size\n!");
        fprintf(stderr, "Error %d: %s \n", errno, strerror(errno));
        return -2;
    }
    // no errors getting device info
    else {
        deviceInfoObj->sectorCount = deviceInfoObj->deviceSize / deviceInfoObj->sectorSize;
        fprintf(stdout, "Device is %llu bytes = %.2f MiB = %.2f GiB large \n",
                (long long unsigned)deviceInfoObj->deviceSize,
                (double)(deviceInfoObj->deviceSize/1024.0/1024.0),
                (double)(deviceInfoObj->deviceSize/1024.0/1024.0/1024.0) );
        fprintf(stdout, "Sector info: %llu bytes/sector, %llu sectors\n\n", 
                (long long unsigned)deviceInfoObj->sectorSize, 
                (long long unsigned)deviceInfoObj->sectorCount );

    } 

    // done getting device info, time to close
    if ( close(device_fd) ) {
        // don't really care if there's an error closing device because it can
        // still be considered effectively closed, but it's good practice to 
        // check for errors anyway
        fprintf(stderr, "\nError %d closing device: %s \n",
                errno, strerror(errno) );
        return -4;
    }

    return 0;

}

//////////////////////////////////////////////////////////////////////////
// Function    : DISKIO_iReadDisk()
// Description : Reads a specified number of sectors from a device
// Parameters  : char *filename - The name of the device file
//               uint8_t *buff - Pointer to the block of memory for which to 
//                               read in the bytes
//               uint32_t sector - Which sector to start reading
//               uint32_t numSectors - Number of sectors to read
//               DeviceInfoType *deviceInfoObj - Object that holds the 
//                                               device information
// Returns     : int - 0 if success, negative value otherwise
//////////////////////////////////////////////////////////////////////////
int DISKIO_iReadDisk(char *filename, uint8_t *buff, uint32_t sector, 
                     uint32_t numSectors, DeviceInfoType *deviceInfoObj) {

    long int offset;
    uint64_t bytesExpected, bytesRead;
    FILE *device_fp;

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
    
    device_fp = fopen(filename, "r");
    if ( NULL == device_fp ) {
        fprintf(stderr, "\nCould not open %s to read!", filename);
        return -3;
    }
        
    fprintf(stdout, "\nReading from %s ...\n", filename);
    offset = (long int)(sector * deviceInfoObj->sectorSize);
    if ( fseek(device_fp, offset, SEEK_SET) ) {
        fprintf(stderr, "\nError finding sectors to read!\n");
        return -1;
    }
    bytesExpected = numSectors * deviceInfoObj->sectorSize;
    bytesRead = (uint64_t)fread(buff, sizeof(uint8_t), bytesExpected, device_fp);
    if ( bytesExpected != bytesRead ) {
        fprintf(stderr, "\nError: %llu bytes read but expected number of bytes is %llu\n"
                "Please try again.",
                (long long unsigned)bytesRead,
                (long long unsigned)bytesExpected );
        return -2;
    }

    // done reading, time to close
    if ( fclose(device_fp) ) {
        fprintf(stderr, "\nError closing file %s", filename);
        return -4;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Function    : DISKIO_iWriteDisk()
// Description : Writes data to a specified number of sectors in a device
// Parameters  : char *filename - The name of the device file
//               uint8_t *buff - Pointer to the block of memory for which to 
//                               read in the bytes
//               uint32_t sector - Which sector to start reading
//               uint32_t numSectors - Number of sectors to read
//               DeviceInfoType *deviceInfoObj - Object that holds the 
//                                               device information
// Returns     : int - 0 if success, negative value otherwise
//////////////////////////////////////////////////////////////////////////
int DISKIO_iWriteDisk(char *filename, uint8_t *buff, uint32_t sector, uint32_t numSectors, 
                      DeviceInfoType *deviceInfoObj) {

    long int offset;
    uint64_t bytesExpected, bytesWritten;
    FILE *device_fp;

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
    
    device_fp = fopen(filename, "w");
    if ( NULL == device_fp ) {
        fprintf(stderr, "\nCould not open %s to write!\n", filename);
        return -3;
    }
    fprintf(stdout, "\nWriting to %s ...\n", filename);
    offset = (long int)(sector * deviceInfoObj->sectorSize);
    if ( fseek(device_fp, offset, SEEK_SET) ) {
        fprintf(stderr, "\nError finding sectors to write!\n");
        return -1;
    }
    bytesExpected = numSectors * deviceInfoObj->sectorSize;
    bytesWritten = (uint64_t)fwrite(buff, sizeof(uint8_t), bytesExpected, device_fp);
    if ( bytesExpected != bytesWritten ) {
        fprintf(stderr, "\nError: %llu bytes written but expected number of bytes is %llu\n"
                "Please try again.",
                (long long unsigned)bytesWritten,
                (long long unsigned)bytesExpected );
        return -2;
    }
    
    // done reading, time to close
    if ( fclose(device_fp) ) {
        fprintf(stderr, "\nError closing file %s\n", filename);
        return -4;
    }

    return 0;

}

//////////////////////////////////////////////////////////////////////////
// Function    : DISKIO_iReadPacket()
// Description : Reads packets from device
// Parameters  : FILE *fp - The file pointer to the device
//               uint8_t *buff - Pointer to the block of memory for which to 
//                               read in the bytes
//               uint64_t startPacketIndex - Packet to start reading at
//               uint16_t packetSize - Number of bytes per packet
//               uint64_t numPackets - Number of packets to read
//               DeviceInfoType *deviceInfoObj - Object that holds the 
//                                               device information
// Returns     : int - 0 if success, negative value otherwise
//////////////////////////////////////////////////////////////////////////
int DISKIO_iReadPacket(FILE *device_fp, uint8_t *buff, uint64_t startPacketIndex, 
                       uint16_t packetSize, uint64_t numPackets,
                       DeviceInfoType *deviceInfoObj) {

    long int startSector;
    long int offset;
    uint64_t numPacketsRead;

    // turn off output buffering
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    // Adding 1 because we start reading packets after the first sector
    // (the configuration sector)
    startSector = (long int)(startPacketIndex * packetSize / deviceInfoObj->sectorSize + 1);
    offset = (long int)((startPacketIndex * packetSize) % deviceInfoObj->sectorSize );

    // position file pointer to correct sector, then move the pointer to 
    // any additional offset from there. Faster than doing 
    // fseek(fp, deviceInfo->sectorSize + startPacketIndex * packetSize, SEEK_SET)
    if ( fseek(device_fp, startSector, SEEK_SET) ) {
        fprintf(stderr, "Error setting file pointer to read packets\n");
        return -1;
    }
    if ( fseek(device_fp, offset, SEEK_CUR) ) {
        fprintf(stderr, "Error setting file pointer to read packets\n");
        return -2;
    }

    numPacketsRead = (uint64_t)fread(buff, sizeof(uint8_t) * packetSize,  
                                     numPackets, device_fp);
    if ( numPacketsRead != numPackets ) {
        fprintf(stderr, "Error reading packets: %llu packets requested but"
               " %llu packets read", 
               (long long unsigned)numPackets,
               (long long unsigned)numPacketsRead);
        return -3;
    }

    return 0;

}