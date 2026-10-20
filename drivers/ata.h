#ifndef ERAN_DRIVERS_ATA_H
#define ERAN_DRIVERS_ATA_H

#include <stdint.h>
#include <stddef.h>

#define ATA_SECTOR_SIZE 512

void ata_init(void);
int  ata_is_detected(void);
int  ata_read_sector(uint32_t lba, uint8_t *buffer);
int  ata_write_sector(uint32_t lba, const uint8_t *buffer);
void ata_print_status(void);

#endif /* ERAN_DRIVERS_ATA_H */
