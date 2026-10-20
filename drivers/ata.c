#include "ata.h"
#include "../hal/io.h"
#include "vga.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_REG_DATA        (ATA_PRIMARY_IO + 0)
#define ATA_REG_ERROR       (ATA_PRIMARY_IO + 1)
#define ATA_REG_SECCOUNT    (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA_LO      (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA_MID     (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA_HI      (ATA_PRIMARY_IO + 5)
#define ATA_REG_DRIVE_HEAD  (ATA_PRIMARY_IO + 6)
#define ATA_REG_STATUS_CMD  (ATA_PRIMARY_IO + 7)

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01

#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_IDENTIFY  0xEC

static int ata_detected = 0;
static uint32_t ata_sector_count = 0;

static int ata_wait_ready(void) {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(ATA_REG_STATUS_CMD);
        if (!(status & ATA_STATUS_BSY)) {
            return 0; // Drive ready
        }
    }
    return -1; // Timeout
}

static int ata_wait_drq(void) {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(ATA_REG_STATUS_CMD);
        if (status & ATA_STATUS_ERR) {
            return -1;
        }
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) {
            return 0;
        }
    }
    return -1;
}

void ata_init(void) {
    // Select master drive on primary bus
    outb(ATA_REG_DRIVE_HEAD, 0xA0);
    io_wait();

    outb(ATA_REG_SECCOUNT, 0);
    outb(ATA_REG_LBA_LO, 0);
    outb(ATA_REG_LBA_MID, 0);
    outb(ATA_REG_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_REG_STATUS_CMD, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_REG_STATUS_CMD);
    if (status == 0 || status == 0xFF) {
        ata_detected = 0;
        return; // No drive present
    }

    if (ata_wait_drq() != 0) {
        ata_detected = 0;
        return;
    }

    // Read 256 words of IDENTIFY data
    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(ATA_REG_DATA);
    }

    ata_detected = 1;
    // Word 60-61 contains total 28-bit LBA sector count
    ata_sector_count = (uint32_t)identify_buf[60] | ((uint32_t)identify_buf[61] << 16);
}

int ata_is_detected(void) {
    return ata_detected;
}

int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    if (!ata_detected || !buffer) return -1;

    if (ata_wait_ready() != 0) return -1;

    // Send drive and LBA high bits
    outb(ATA_REG_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();

    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_REG_STATUS_CMD, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) return -1;

    uint16_t *target = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        target[i] = inw(ATA_REG_DATA);
    }

    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    if (!ata_detected || !buffer) return -1;

    if (ata_wait_ready() != 0) return -1;

    outb(ATA_REG_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();

    outb(ATA_REG_SECCOUNT, 1);
    outb(ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_REG_STATUS_CMD, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) return -1;

    const uint16_t *source = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_REG_DATA, source[i]);
    }

    // Cache flush command
    outb(ATA_REG_STATUS_CMD, 0xE7);
    ata_wait_ready();

    return 0;
}

void ata_print_status(void) {
    kprintf("===================================================================\n");
    kprintf("                PARALLEL ATA / IDE STORAGE CONTROLLER              \n");
    kprintf("===================================================================\n");
    if (ata_detected) {
        kprintf(" Primary Master Drive : DETECTED & ACTIVE\n");
        kprintf(" Interface Mode       : Programmed I/O (PIO Mode, Port 0x1F0)\n");
        kprintf(" Total Sectors (LBA28): %u sectors\n", ata_sector_count);
        kprintf(" Total Capacity       : %u MB\n", (ata_sector_count * 512) / (1024 * 1024));
    } else {
        kprintf(" Primary Master Drive : NOT DETECTED (Port 0x1F0 floating/offline)\n");
    }
    kprintf("===================================================================\n");
}
