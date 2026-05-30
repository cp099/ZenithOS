#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Read sectors from the hard drive (LBA mode, primary slave drive)
void ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buffer);

// Write sectors to the hard drive (LBA mode, primary slave drive)
void ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buffer);

#endif
