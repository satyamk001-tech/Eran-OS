#ifndef ERAN_HAL_CPU_H
#define ERAN_HAL_CPU_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char vendor[13];
    char brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    int has_fpu;
    int has_tsc;
    int has_apic;
    int has_pae;
    int has_sse;
    int has_sse2;
    int has_sse3;
    int has_sse4_1;
    int has_avx;
    int has_long_mode; // 64-bit x86_64 capability (Bit 29 of 0x80000001 EDX)
    int has_nx;        // Execute Disable bit
} cpu_info_t;

void cpu_init(void);
const cpu_info_t *cpu_get_info(void);
void cpu_print_features(void);

#endif /* ERAN_HAL_CPU_H */
