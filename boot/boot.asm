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
    ; 1. Load Kernel Sectors across Floppy Cylinders/Heads (INT 13h)
    ;    Loads 180 sectors (90 KB) starting at Cylinder 0, Head 0, Sector 2.
    ; --------------------------------------------------------------------------
    mov ax, 0x1000
    mov es, ax
    xor bx, bx                  ; Destination: 0x1000:0x0000 -> physical 0x10000

    mov ch, 0                   ; Cylinder 0
    mov dh, 0                   ; Head 0
    mov cl, 2                   ; Sector 2 (Sector 1 is boot sector)
    mov bp, 180                 ; Read 180 sectors (90 KB kernel capacity)

.read_sector_loop:
    mov ah, 0x02                ; BIOS Read Sectors
    mov al, 1                   ; Read 1 sector per iteration
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Advance destination pointer by 512 bytes (0x20 in segment: 0x20 * 16 = 512)
    mov ax, es
    add ax, 0x20
    mov es, ax

    ; Advance to next sector in standard 1.44MB Floppy geometry (18 sectors/track)
    inc cl
    cmp cl, 19
    jb .loop_dec
    mov cl, 1                   ; Reset to sector 1
    inc dh                      ; Next head
    cmp dh, 2
    jb .loop_dec
    mov dh, 0                   ; Reset to head 0
    inc ch                      ; Next cylinder

.loop_dec:
    dec bp
    jnz .read_sector_loop

    mov si, msg_ok
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

msg_boot: db 'Eran OS Bootloader v0.3.0', ENDL, 0
msg_ok:   db '[BOOT] 180 sectors (90 KB) loaded to 0x10000. Entering PM...', ENDL, 0
msg_err:  db '[FATAL] Disk read error!', ENDL, 0

; ------------------------------------------------------------------------------
; Boot Sector Signature (512 bytes)
; ------------------------------------------------------------------------------
times 510-($-$$) db 0
dw 0AA55h
