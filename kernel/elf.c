#include "elf.h"
#include "gdt.h"
#include "../drivers/vga.h"

extern void enter_user_mode(uint32_t entry_point, uint32_t user_stack);

int elf_validate(const void *buffer, size_t size) {
    if (!buffer || size < sizeof(elf32_header_t)) return 0;

    const elf32_header_t *hdr = (const elf32_header_t *)buffer;
    if (*(const uint32_t *)hdr->e_ident != ELF_MAGIC) return 0;
    if (hdr->e_ident[4] != 1) return 0; // 32-bit architecture
    if (hdr->e_ident[5] != 1) return 0; // Little endian
    if (hdr->e_machine != EM_386) return 0; // x86

    return 1;
}

int elf_load_and_run(const void *buffer, size_t size) {
    if (!elf_validate(buffer, size)) {
        kprintf("elf: Invalid ELF32 binary executable!\n");
        return -1;
    }

    const elf32_header_t *hdr = (const elf32_header_t *)buffer;
    const uint8_t *file_bytes = (const uint8_t *)buffer;

    kprintf("[ELF] Loading 32-bit executable...\n");
    kprintf("[ELF] Entry Point: 0x%x, Program Headers: %u\n", hdr->e_entry, hdr->e_phnum);

    for (uint16_t i = 0; i < hdr->e_phnum; i++) {
        const elf32_program_header_t *ph = (const elf32_program_header_t *)(file_bytes + hdr->e_phoff + i * hdr->e_phentsize);
        if (ph->p_type == PT_LOAD) {
            kprintf("  -> Segment %d: VAddr 0x%x, FileSz %u bytes, MemSz %u bytes\n",
                    i, ph->p_vaddr, ph->p_filesz, ph->p_memsz);

            // Copy file segment to virtual address
            uint8_t *dst = (uint8_t *)ph->p_vaddr;
            const uint8_t *src = file_bytes + ph->p_offset;
            for (uint32_t b = 0; b < ph->p_filesz; b++) {
                dst[b] = src[b];
            }

            // Zero remaining memory (BSS section)
            for (uint32_t b = ph->p_filesz; b < ph->p_memsz; b++) {
                dst[b] = 0;
            }
        }
    }

    // Set kernel TSS stack for safe interrupt returns from user mode
    tss_set_stack(0x00090000);

    // Prepare user mode stack at 0x00800000 (8 MB Mark)
    uint32_t user_stack = 0x00800000;

    kprintf("[ELF] Executing binary in Ring 3 User Mode (CS=0x1B, DS=0x23)...\n");

    // Execute user program
    enter_user_mode(hdr->e_entry, user_stack);

    return 0;
}

void elf_print_info(const void *buffer, size_t size) {
    if (!elf_validate(buffer, size)) {
        kprintf("elf: Invalid ELF32 binary!\n");
        return;
    }

    const elf32_header_t *hdr = (const elf32_header_t *)buffer;
    kprintf("===================================================================\n");
    kprintf("                    ELF32 EXECUTABLE BINARY HEADER                 \n");
    kprintf("===================================================================\n");
    kprintf(" Format Identifier     : ELF32 Standard (Little-Endian)\n");
    kprintf(" Machine Target        : Intel 80386 (x86 32-bit Protected Mode)\n");
    kprintf(" Entry Point Linear    : 0x%x\n", hdr->e_entry);
    kprintf(" Program Header Offset : 0x%x (%u headers)\n", hdr->e_phoff, hdr->e_phnum);
    kprintf(" Section Header Offset : 0x%x (%u headers)\n", hdr->e_shoff, hdr->e_shnum);
    kprintf(" Execution Privilege   : Ring 3 User Space (DPL=3)\n");
    kprintf("===================================================================\n");
}
