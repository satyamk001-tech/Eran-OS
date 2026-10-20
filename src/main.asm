; ==============================================================================
; Eran OS - Master Boot Sector (Stage 1 Bootloader)
;
; Target Architecture: x86 (16-bit Real Mode)
; Memory Origin:       0x7C00 (standard BIOS load address)
; Output Format:       512-byte raw binary with boot signature (0x55, 0xAA)
; ==============================================================================

org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

start:
    ; Guarantee code segment CS is normalized to 0x0000.
    ; Certain BIOS implementations jump to 0x07C0:0x0000 instead of 0x0000:0x7C00.
    jmp 0x0000:.init_cs

.init_cs:
    ; Disable interrupts during critical segment register and stack setup
    cli
    cld                     ; Clear direction flag: ensure string instructions increment SI/DI

    xor ax, ax
    mov ds, ax              ; Data Segment = 0x0000
    mov es, ax              ; Extra Segment = 0x0000
    mov ss, ax              ; Stack Segment = 0x0000
    mov sp, 0x7C00          ; Stack Pointer = 0x7C00 (stack grows downward below bootloader)

    ; Save BIOS boot drive number passed in DL (0x00=floppy, 0x80=first hard disk)
    ; This is required for Phase 2 INT 13h disk reads.
    mov [boot_drive], dl

    sti                     ; Re-enable interrupts

    ; Initialize COM1 serial port (0x3F8) for headless diagnostics & QEMU testing
    call serial_init

    ; Print startup banner to screen (BIOS INT 10h) and serial port (COM1)
    mov si, msg_banner
    call puts

    mov si, msg_status
    call puts

.hang:
    hlt                     ; Halt processor until next interrupt
    jmp .hang               ; Prevent fall-through if interrupted

; ------------------------------------------------------------------------------
; serial_init: Initialize COM1 UART at standard I/O port 0x3F8
; Configuration: 38400 baud, 8 data bits, no parity, 1 stop bit (8N1), FIFO enabled
; ------------------------------------------------------------------------------
serial_init:
    push ax
    push dx

    mov dx, 0x3F9           ; Interrupt Enable Register (IER)
    xor al, al              ; Disable all interrupts
    out dx, al

    mov dx, 0x3FB           ; Line Control Register (LCR)
    mov al, 0x80            ; Enable DLAB (Divisor Latch Access Bit)
    out dx, al

    mov dx, 0x3F8           ; Divisor Latch Low (DLL)
    mov al, 0x03            ; 115200 / 3 = 38400 baud
    out dx, al

    mov dx, 0x3F9           ; Divisor Latch High (DLH)
    xor al, al
    out dx, al

    mov dx, 0x3FB           ; Line Control Register
    mov al, 0x03            ; 8 bits, no parity, 1 stop bit (8N1), DLAB disabled
    out dx, al

    mov dx, 0x3FA           ; FIFO Control Register (FCR)
    mov al, 0xC7            ; Enable FIFO, clear TX/RX queues, 14-byte threshold
    out dx, al

    mov dx, 0x3FC           ; Modem Control Register (MCR)
    mov al, 0x03            ; Assert RTS and DTR
    out dx, al

    pop dx
    pop ax
    ret

; ------------------------------------------------------------------------------
; serial_putc: Transmit a single byte in AL over COM1 serial
; ------------------------------------------------------------------------------
serial_putc:
    push dx
    push ax                 ; Preserve character

    mov dx, 0x3FD           ; Line Status Register (LSR)
.wait_tx:
    in al, dx
    test al, 0x20           ; Check bit 5: Transmitter Holding Register Empty (THRE)
    jz .wait_tx

    pop ax                  ; Restore character
    mov dx, 0x3F8           ; Transmit Data Register
    out dx, al

    pop dx
    ret

; ------------------------------------------------------------------------------
; puts: Prints null-terminated string at DS:SI to screen (INT 10h) and COM1 serial
; Parameters:
;   DS:SI -> Pointer to null-terminated ASCII string
; ------------------------------------------------------------------------------
puts:
    push si
    push ax

.loop:
    lodsb                   ; AL = [DS:SI], SI++ (guaranteed by CLD)
    or al, al               ; Check for null terminator (0x00)
    jz .done

    ; Teletype output to BIOS screen (INT 10h, AH=0x0E)
    push ax
    mov ah, 0x0E
    mov bh, 0x00            ; Video page 0
    mov bl, 0x07            ; Character attribute: light gray on black
    int 0x10
    pop ax

    ; Output same character to COM1 serial port
    call serial_putc

    jmp .loop

.done:
    pop ax
    pop si
    ret

; ------------------------------------------------------------------------------
; Data Section
; ------------------------------------------------------------------------------
boot_drive: db 0

msg_banner:
    db '=====================================================', ENDL
    db '  Eran OS - Stage 1 Bootloader v0.1.0', ENDL
    db '=====================================================', ENDL, 0

msg_status:
    db '[BOOT] CPU real mode initialized (CS:0000 SP:7C00).', ENDL
    db '[BOOT] BIOS INT 10h display and COM1 serial active.', ENDL
    db '[BOOT] Drive ID preserved for Stage 2 disk load.', ENDL
    db '[BOOT] System halted. Ready for Phase 2.', ENDL, 0

; ------------------------------------------------------------------------------
; Boot Sector Padding & Signature
; Standard requirement: sector must be exactly 512 bytes ending in 0xAA55
; ------------------------------------------------------------------------------
times 510-($-$$) db 0
dw 0AA55h
