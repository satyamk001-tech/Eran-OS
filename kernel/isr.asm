; ==============================================================================
; Eran OS - Low-Level Interrupt Service Routine (ISR) Stubs
; Architecture: 32-bit x86 Protected Mode
; ==============================================================================

bits 32

extern isr_handler
extern irq_handler

; ------------------------------------------------------------------------------
; Macro for exceptions that DO NOT push an error code onto the stack
; ------------------------------------------------------------------------------
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0        ; Dummy error code to keep stack frame consistent
    push dword %1       ; Interrupt number
    jmp isr_common_stub
%endmacro

; ------------------------------------------------------------------------------
; Macro for exceptions that DO push an error code onto the stack
; ------------------------------------------------------------------------------
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1       ; Interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; ------------------------------------------------------------------------------
; Macro for Hardware IRQs (Master: 32-39, Slave: 40-47)
; ------------------------------------------------------------------------------
%macro IRQ 2
global irq%1
irq%1:
    push dword 0        ; Dummy error code
    push dword %2       ; Interrupt number (32 + %1)
    jmp irq_common_stub
%endmacro

; CPU Exceptions 0 - 31
ISR_NOERRCODE 0         ; Divide-by-zero
ISR_NOERRCODE 1         ; Debug
ISR_NOERRCODE 2         ; Non-maskable Interrupt (NMI)
ISR_NOERRCODE 3         ; Breakpoint
ISR_NOERRCODE 4         ; Overflow
ISR_NOERRCODE 5         ; Bound Range Exceeded
ISR_NOERRCODE 6         ; Invalid Opcode
ISR_NOERRCODE 7         ; Device Not Available
ISR_ERRCODE   8         ; Double Fault
ISR_NOERRCODE 9         ; Coprocessor Segment Overrun
ISR_ERRCODE   10        ; Invalid TSS
ISR_ERRCODE   11        ; Segment Not Present
ISR_ERRCODE   12        ; Stack-Segment Fault
ISR_ERRCODE   13        ; General Protection Fault
ISR_ERRCODE   14        ; Page Fault
ISR_NOERRCODE 15        ; Reserved
ISR_NOERRCODE 16        ; x87 Floating-Point Exception
ISR_ERRCODE   17        ; Alignment Check
ISR_NOERRCODE 18        ; Machine Check
ISR_NOERRCODE 19        ; SIMD Floating-Point Exception
ISR_NOERRCODE 20        ; Virtualization Exception
ISR_ERRCODE   21        ; Control Protection Exception
ISR_NOERRCODE 22        ; Reserved
ISR_NOERRCODE 23        ; Reserved
ISR_NOERRCODE 24        ; Reserved
ISR_NOERRCODE 25        ; Reserved
ISR_NOERRCODE 26        ; Reserved
ISR_NOERRCODE 27        ; Reserved
ISR_NOERRCODE 28        ; Hypervisor Injection Exception
ISR_ERRCODE   29        ; VMM Communication Exception
ISR_ERRCODE   30        ; Security Exception
ISR_NOERRCODE 31        ; Reserved

; Hardware IRQs 0 - 15 (Remapped to vectors 32 - 47)
IRQ 0, 32               ; PIT Timer
IRQ 1, 33               ; Keyboard
IRQ 2, 34               ; Cascade
IRQ 3, 35               ; COM2
IRQ 4, 36               ; COM1
IRQ 5, 37               ; LPT2
IRQ 6, 38               ; Floppy Disk
IRQ 7, 39               ; LPT1 / Spurious
IRQ 8, 40               ; CMOS RTC
IRQ 9, 41               ; Free / ACPI
IRQ 10, 42              ; Free / Network
IRQ 11, 43              ; Free / USB
IRQ 12, 44              ; PS/2 Mouse
IRQ 13, 45              ; FPU Coprocessor
IRQ 14, 46              ; Primary ATA Hard Disk
IRQ 15, 47              ; Secondary ATA Hard Disk

; ------------------------------------------------------------------------------
; Common ISR handler stub
; ------------------------------------------------------------------------------
isr_common_stub:
    pusha               ; Pushes EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX

    mov ax, ds          ; Lower 16-bits of EAX = DS
    push eax            ; Save DS descriptor

    mov ax, 0x10        ; Load Kernel Data Segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; Pass pointer to registers_t struct on stack
    call isr_handler
    add esp, 4          ; Clean up parameter

    pop eax             ; Restore original data segment descriptor
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                ; Restore general registers
    add esp, 8          ; Clean up error code and interrupt number
    iret                ; Return from interrupt

; ------------------------------------------------------------------------------
; Common IRQ handler stub
; ------------------------------------------------------------------------------
irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret
