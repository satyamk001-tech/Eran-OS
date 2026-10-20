#include "gdt.h"
#include <stddef.h>

struct gdt_entry_struct {
    uint16_t limit_low;           // The lower 16 bits of the limit
    uint16_t base_low;            // The lower 16 bits of the base
    uint8_t  base_middle;         // The next 8 bits of the base
    uint8_t  access;              // Access flags
    uint8_t  granularity;         // Granularity + high 4 bits of limit
    uint8_t  base_high;           // The last 8 bits of the base
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct {
    uint16_t limit;               // The upper 16 bits of all selector limits
    uint32_t base;                // The address of the first gdt_entry_t struct
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

#define GDT_ENTRIES 6
static gdt_entry_t gdt_entries[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss_entry;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (uint16_t)(base & 0xFFFF);
    gdt_entries[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt_entries[num].base_high   = (uint8_t)((base >> 24) & 0xFF);

    gdt_entries[num].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt_entries[num].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access      = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t) - 1;

    gdt_set_gate(num, base, limit, 0x89, 0x00);

    // Zero the TSS structure
    uint8_t *p = (uint8_t *)&tss_entry;
    for (size_t i = 0; i < sizeof(tss_entry_t); i++) {
        p[i] = 0;
    }

    tss_entry.ss0  = ss0;
    tss_entry.esp0 = esp0;
    tss_entry.cs   = 0x08 | 3;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x10 | 3;
    tss_entry.iomap_base = sizeof(tss_entry_t);
}

void tss_set_stack(uint32_t kernel_esp) {
    tss_entry.esp0 = kernel_esp;
}

void gdt_init(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 0x00: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 0x08: Kernel Code Segment (Base=0, Limit=4GB, Ring 0, Exec/Read)
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // 0x10: Kernel Data Segment (Base=0, Limit=4GB, Ring 0, Read/Write)
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);

    // 0x18: User Code Segment (Base=0, Limit=4GB, Ring 3, Exec/Read, Access 0xFA)
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);

    // 0x20: User Data Segment (Base=0, Limit=4GB, Ring 3, Read/Write, Access 0xF2)
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

    // 0x28: Task State Segment (TSS)
    write_tss(5, 0x10, 0x00090000);

    // Load GDT pointer into CPU
    __asm__ volatile ("lgdt (%0)" : : "r"(&gdt_ptr));

    // Reload segment registers to use newly loaded GDT
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $.flush_cs\n"
        ".flush_cs:\n"
        : : : "eax"
    );

    // Load Task Register (TSS selector = 0x28 = 5 * 8)
    __asm__ volatile ("ltr %%ax" : : "a"(0x28));
}
