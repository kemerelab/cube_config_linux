#ifndef DISKIO_LINUX_H
#define DISKIO_LINUX_H

#include <stdio.h>
#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
//                      Public Data Types
//////////////////////////////////////////////////////////////////////////
typedef enum {
    READ_ACCESS,
    WRITE_ACCESS,
    EXECUTE_ACCESS
} FilePermissionType;

typedef struct {
    // probably don't need integers this large if we're only handling 
    // SD cards but we'll use these just in case of overflow, eh?
    uint64_t sectorCount;
    uint64_t sectorSize;
    uint64_t deviceSize;
} DeviceInfoType;

//////////////////////////////////////////////////////////////////////////
//                    Public Function Prototypes
//////////////////////////////////////////////////////////////////////////
int DISKIO_iCheckFileAccess(char *filename, FilePermissionType permission);

int DISKIO_iGetDeviceInfo(char *filename, DeviceInfoType *deviceInfoObj);

int DISKIO_iReadDisk(char *filename, uint8_t *buff, uint32_t sector, 
                     uint32_t numSectors, DeviceInfoType *deviceInfoObj);

int DISKIO_iWriteDisk(char *filename, uint8_t *buff, uint32_t sector, 
                      uint32_t numSectors, DeviceInfoType *deviceInfoObj);

int DISKIO_iReadPacket(FILE *device_fp, uint8_t *buff, uint64_t startPacketIndex, 
                       uint16_t packetSize, uint64_t numPackets, 
                       DeviceInfoType *deviceInfoObj);

#endif // DISKIO_LINUX_H