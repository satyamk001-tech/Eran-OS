; ==============================================================================
; Eran OS - Kernel Entry Point
; Architecture: 32-bit Protected Mode x86
; Memory Location: 0x10000
; ==============================================================================

bits 32

global _start
extern kmain

section .entry
_start:
    ; Establish segment selectors (Kernel Data Segment = 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup 32-bit kernel stack (safe area below 1MB mark)
    mov esp, 0x00090000
    mov ebp, esp

    ; Invoke C kernel main function
    call kmain

    ; Halt if kmain ever returns
.hang:
    cli
    hlt
    jmp .hang
