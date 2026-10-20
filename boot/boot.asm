; ==============================================================================
; Eran OS - Master Boot Sector & Stage 2 Transition
; Architecture: x86 (16-bit Real Mode -> 32-bit Protected Mode)
; Memory Origin: 0x7C00
; Target Kernel Load Address: 0x10000 (Physical RAM)
; ==============================================================================

org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

start:
    ; Normalize CS to 0x0000
    jmp 0x0000:init

init:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl
    sti

    call serial_init
    mov si, msg_boot
    call puts

    ; --------------------------------------------------------------------------
    ; 1. Load Kernel Sectors across Floppy Cylinder/Head Boundaries (INT 13h)
    ; --------------------------------------------------------------------------
    ; Track 0, Head 0: Read remaining 17 sectors (Sectors 2 to 18)
    mov ax, 0x1000
    mov es, ax
    xor bx, bx                  ; Destination: 0x1000:0x0000 -> 0x10000
    mov ah, 0x02
    mov al, 17
    mov ch, 0                   ; Cylinder 0
    mov cl, 2                   ; Sector 2
    mov dh, 0                   ; Head 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_ok1
    call puts

    ; Track 0, Head 1: Read 18 sectors (Sectors 1 to 18)
    mov ax, 0x1000 + (17 * 512 / 16)
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 18
    mov ch, 0                   ; Cylinder 0
    mov cl, 1                   ; Sector 1
    mov dh, 1                   ; Head 1
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_ok2
    call puts

    ; Track 1, Head 0: Read 18 sectors (Sectors 1 to 18) -> Total 53 sectors = 27 KB
    mov ax, 0x1000 + ((17 + 18) * 512 / 16)
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 18
    mov ch, 1                   ; Cylinder 1
    mov cl, 1                   ; Sector 1
    mov dh, 0                   ; Head 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_ok3
    call puts

    ; --------------------------------------------------------------------------
    ; 2. Enable Fast A20 Gate
    ; --------------------------------------------------------------------------
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; --------------------------------------------------------------------------
    ; 3. Load GDT and Enter 32-bit Protected Mode
    ; --------------------------------------------------------------------------
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1                   ; Set CR0.PE = 1
    mov cr0, eax

    ; Far jump to flush 16-bit pipeline and set CS to 32-bit Code Segment
    jmp dword 0x08:pm_entry

disk_error:
    mov si, msg_err
    call puts
.hang:
    hlt
    jmp .hang

serial_init:
    push ax
    push dx
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03                ; 38400 baud
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03                ; 8N1
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7                ; FIFO
    out dx, al
    mov dx, 0x3FC
    mov al, 0x03                ; RTS/DTR
    out dx, al
    pop dx
    pop ax
    ret

serial_putc:
    push dx
    push ax
    mov dx, 0x3FD
.wait_tx:
    in al, dx
    test al, 0x20
    jz .wait_tx
    pop ax
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

puts:
    push si
    push ax
.loop:
    lodsb
    or al, al
    jz .done
    push ax
    mov ah, 0x0E
    int 0x10
    pop ax
    call serial_putc
    jmp .loop
.done:
    pop ax
    pop si
    ret

; ------------------------------------------------------------------------------
; 32-bit Protected Mode Entry Point
; ------------------------------------------------------------------------------
bits 32
pm_entry:
    mov ax, 0x10                ; 32-bit Kernel Data Segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x00090000         ; Safe stack below 640 KB
    jmp 0x00010000              ; Jump directly to kmain entry point

; ------------------------------------------------------------------------------
; Global Descriptor Table (GDT)
; ------------------------------------------------------------------------------
bits 16
align 8
gdt_start:
    dd 0, 0                     ; Null Descriptor
    ; 32-bit Kernel Code Segment Descriptor (Selector 0x08)
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    ; 32-bit Kernel Data Segment Descriptor (Selector 0x10)
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

boot_drive: db 0

msg_boot: db 'Eran OS Bootloader v0.2.0', ENDL, 0
msg_ok1:  db '[BOOT] Track 0 Head 0 loaded.', ENDL, 0
msg_ok2:  db '[BOOT] Track 0 Head 1 loaded.', ENDL, 0
msg_ok3:  db '[BOOT] Track 1 Head 0 loaded. Total 53 sectors.', ENDL, 0
msg_err:  db '[FATAL] Disk read error!', ENDL, 0

; ------------------------------------------------------------------------------
; Boot Sector Signature (512 bytes)
; ------------------------------------------------------------------------------
times 510-($-$$) db 0
dw 0AA55h
