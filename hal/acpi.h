#ifndef ERAN_HAL_ACPI_H
#define ERAN_HAL_ACPI_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    int acpi_supported;
    char oem_id[7];
    uint32_t rsdt_addr;
    uint32_t fadt_addr;
    uint32_t pm1a_cnt;
    int is_ac_connected;
    int battery_percentage;
    const char *battery_state;
} acpi_status_t;

void acpi_init(void);
int  acpi_is_supported(void);
void acpi_print_status(void);
void acpi_print_battery(void);
void acpi_poweroff(void);
void acpi_reboot(void);

#endif /* ERAN_HAL_ACPI_H */
