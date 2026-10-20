/* ==============================================================================
 * Eran OS - Kernel Linker Script
 * Target: 32-bit x86 ELF / Freestanding Raw Binary
 * Load Address: 0x10000 (Physical RAM)
 * ============================================================================== */

ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    . = 0x10000;

    .text : ALIGN(4K)
    {
        *(.entry)
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    _kernel_end = .;
}
