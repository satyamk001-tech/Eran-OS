#include "cpu.h"
#include "../drivers/vga.h"

static cpu_info_t cpu_info;

static inline void cpuid(uint32_t code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(code));
}

void cpu_init(void) {
    uint32_t eax, ebx, ecx, edx;

    // 1. Get Vendor String (EAX=0)
    cpuid(0, &eax, &ebx, &ecx, &edx);
    ((uint32_t *)cpu_info.vendor)[0] = ebx;
    ((uint32_t *)cpu_info.vendor)[1] = edx;
    ((uint32_t *)cpu_info.vendor)[2] = ecx;
    cpu_info.vendor[12] = '\0';

    // 2. Get Features (EAX=1)
    cpuid(1, &eax, &ebx, &ecx, &edx);
    cpu_info.stepping = eax & 0x0F;
    cpu_info.model = (eax >> 4) & 0x0F;
    cpu_info.family = (eax >> 8) & 0x0F;

    cpu_info.has_fpu    = (edx & (1 << 0)) != 0;
    cpu_info.has_tsc    = (edx & (1 << 4)) != 0;
    cpu_info.has_pae    = (edx & (1 << 6)) != 0;
    cpu_info.has_apic   = (edx & (1 << 9)) != 0;
    cpu_info.has_sse    = (edx & (1 << 25)) != 0;
    cpu_info.has_sse2   = (edx & (1 << 26)) != 0;

    cpu_info.has_sse3   = (ecx & (1 << 0)) != 0;
    cpu_info.has_sse4_1 = (ecx & (1 << 19)) != 0;
    cpu_info.has_avx    = (ecx & (1 << 28)) != 0;

    // 3. Check Extended Functions for Long Mode (64-bit)
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    uint32_t max_ext = eax;

    cpu_info.has_long_mode = 0;
    cpu_info.has_nx = 0;

    if (max_ext >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        cpu_info.has_long_mode = (edx & (1 << 29)) != 0;
        cpu_info.has_nx = (edx & (1 << 20)) != 0;
    }

    // 4. Get Processor Brand String (0x80000002 to 0x80000004)
    for (int i = 0; i < 48; i++) cpu_info.brand[i] = '\0';
    if (max_ext >= 0x80000004) {
        uint32_t *brand_ptr = (uint32_t *)cpu_info.brand;
        for (uint32_t op = 0x80000002; op <= 0x80000004; op++) {
            cpuid(op, &eax, &ebx, &ecx, &edx);
            *brand_ptr++ = eax;
            *brand_ptr++ = ebx;
            *brand_ptr++ = ecx;
            *brand_ptr++ = edx;
        }
    } else {
        const char *fallback = "x86 Compatible Processor";
        for (int i = 0; fallback[i] != '\0'; i++) cpu_info.brand[i] = fallback[i];
    }
}

const cpu_info_t *cpu_get_info(void) {
    return &cpu_info;
}

void cpu_print_features(void) {
    kprintf("===================================================================\n");
    kprintf("                CPU ARCHITECTURE & EXTENDED INSTRUCTION SET        \n");
    kprintf("===================================================================\n");
    kprintf(" Vendor ID String      : %s\n", cpu_info.vendor);
    kprintf(" Processor Model       : %s\n", cpu_info.brand);
    kprintf(" Family / Model / Step : Family %u, Model %u, Stepping %u\n",
            cpu_info.family, cpu_info.model, cpu_info.stepping);
    kprintf(" Current Execution     : 32-bit Protected Mode (IA-32 Ring 0/3)\n");
    kprintf(" 64-bit Long Mode (LM) : %s\n",
            cpu_info.has_long_mode ? "YES (x86_64 Capable Hardware Target)" : "NO (Pure 32-bit Hardware)");
    kprintf(" Hardware Features     : [FPU:%s] [TSC:%s] [APIC:%s] [PAE:%s]\n",
            cpu_info.has_fpu ? "YES" : "NO",
            cpu_info.has_tsc ? "YES" : "NO",
            cpu_info.has_apic ? "YES" : "NO",
            cpu_info.has_pae ? "YES" : "NO");
    kprintf(" Vector SIMD Sets      : [SSE:%s] [SSE2:%s] [SSE3:%s] [SSE4.1:%s] [AVX:%s]\n",
            cpu_info.has_sse ? "YES" : "NO",
            cpu_info.has_sse2 ? "YES" : "NO",
            cpu_info.has_sse3 ? "YES" : "NO",
            cpu_info.has_sse4_1 ? "YES" : "NO",
            cpu_info.has_avx ? "YES" : "NO");
    kprintf(" Memory Protection     : [NX/XD No-Execute: %s]\n", cpu_info.has_nx ? "YES" : "NO");
    kprintf("===================================================================\n");
}
