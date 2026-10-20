#ifndef ERAN_HAL_LONGMODE_H
#define ERAN_HAL_LONGMODE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int cpu_supported;
    int pae_enabled;
    int lme_ready;
    uint32_t pml4_address;
    const char *status_message;
} longmode_status_t;

void longmode_init(void);
const longmode_status_t *longmode_get_status(void);
void longmode_print_status(void);
int  longmode_test_transition(void);

#endif /* ERAN_HAL_LONGMODE_H */
