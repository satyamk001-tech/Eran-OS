#ifndef ERAN_KERNEL_ELF_H
#define ERAN_KERNEL_ELF_H

#include <stdint.h>
#include <stddef.h>

#define EI_NIDENT       16
#define ELF_MAGIC       0x464C457F // \x7F 'E' 'L' 'F' (Little Endian)

#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3

#define EM_386          3          // Intel 80386

#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_NOTE         4

#define PF_X            1
#define PF_W            2
#define PF_R            4

typedef struct {
    uint8_t   e_ident[EI_NIDENT];
    uint16_t  e_type;
    uint16_t  e_machine;
    uint32_t  e_version;
    uint32_t  e_entry;
    uint32_t  e_phoff;
    uint32_t  e_shoff;
    uint32_t  e_flags;
    uint16_t  e_ehsize;
    uint16_t  e_phentsize;
    uint16_t  e_phnum;
    uint16_t  e_shentsize;
    uint16_t  e_shnum;
    uint16_t  e_shstrndx;
} __attribute__((packed)) elf32_header_t;

typedef struct {
    uint32_t  p_type;
    uint32_t  p_offset;
    uint32_t  p_vaddr;
    uint32_t  p_paddr;
    uint32_t  p_filesz;
    uint32_t  p_memsz;
    uint32_t  p_flags;
    uint32_t  p_align;
} __attribute__((packed)) elf32_program_header_t;

int elf_validate(const void *buffer, size_t size);
int elf_load_and_run(const void *buffer, size_t size);
void elf_print_info(const void *buffer, size_t size);

#endif /* ERAN_KERNEL_ELF_H */
