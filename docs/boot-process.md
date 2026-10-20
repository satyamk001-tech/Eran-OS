# Eran OS Boot Process Specification

## 1. Overview and Target Platform

The boot process of Eran OS starts in **x86 16-bit Real Mode** under IBM PC / AT compatible legacy BIOS firmware. The primary boot medium is initially structured as a 1.44 MB floppy disk image (or USB drive in floppy/hard disk emulation mode).

---

## 2. Real Mode Boot Environment at BIOS Handover

When the computer powers on, the BIOS runs Power-On Self-Test (POST), enumerates bootable media, and reads the first 512-byte sector (LBA 0) of the selected boot device into physical memory at address `0x0000:0x7C00`.

### Handover Register State
- **`DL`:** Contains the BIOS boot drive number:
  - `0x00`: First floppy diskette drive.
  - `0x80`: First hard disk / USB storage drive.
- **`CS:IP`:** Either `0x0000:0x7C00` or `0x07C0:0x0000` depending on the BIOS vendor. Both resolve to physical address `0x7C00` (`CS * 16 + IP`), but an unnormalized segment causes subtle pointer calculations issues.
- **`DS`, `ES`, `SS`, `SP`:** Undefined and potentially dirty.
- **Direction Flag (DF):** Undefined. If set, string instructions (`lodsb`, `movsb`) will decrement rather than increment pointers.

---

## 3. Early Physical Memory Layout (First 1 MB)

```
+-----------------------------------+ 0x100000 (1 MB)
| BIOS Extended Memory / ROM Area   |
+-----------------------------------+ 0x0A0000 (640 KB)
| Free Conventional RAM             |
+-----------------------------------+ 0x007E00
| Stage 1 Boot Sector (512 bytes)   | Loaded at 0x7C00 - 0x7E00
+-----------------------------------+ 0x007C00
| Stack Area (grows downwards)      | SP initialized to 0x7C00
+-----------------------------------+ 0x000500
| BIOS Data Area (BDA)              | 256 bytes (0x0400 - 0x04FF)
+-----------------------------------+ 0x000400
| Real Mode Interrupt Vector Table  | 1024 bytes (0x0000 - 0x03FF)
+-----------------------------------+ 0x000000
```

---

## 4. Stage 1 Bootloader Implementation Details

The Eran OS Stage 1 bootloader (`src/main.asm`) enforces strict hardware invariants:

### 4.1 Segment Normalization & Far Jump
```nasm
start:
    jmp 0x0000:.init_cs
.init_cs:
```
The far jump immediately forces `CS` to `0x0000` and `IP` to `.init_cs`, guaranteeing consistent segment:offset calculations across all BIOS implementations.

### 4.2 Stack & Register Sanitization
```nasm
    cli                     ; Disable interrupts during stack setup
    cld                     ; Clear Direction Flag (ensures lodsb increments SI)
    xor ax, ax
    mov ds, ax              ; DS = 0x0000
    mov es, ax              ; ES = 0x0000
    mov ss, ax              ; SS = 0x0000
    mov sp, 0x7C00          ; Stack grows downward from 0x7C00
    mov [boot_drive], dl    ; Preserve BIOS boot drive ID
    sti                     ; Re-enable interrupts
```
- Disables interrupts (`cli`) while changing `SS` and `SP`.
- Clears the direction flag (`cld`).
- Places the stack pointer at `0x7C00`. The stack grows down toward `0x0500`, providing ~30 KB of safe stack space without corrupting the bootloader or the BIOS Data Area (`0x0400`).
- Captures register `DL` into `boot_drive`. This ID is required for subsequent BIOS disk reads via `INT 13h`.

### 4.3 Diagnostic COM1 UART Serial Output
To allow automated headless testing and debugging in virtualized environments (QEMU, VirtualBox), the bootloader initializes COM1 serial port (`0x3F8`) in 16-bit real mode:
- Disables UART interrupts (`0x3F9 = 0x00`).
- Sets Divisor Latch Access Bit (`0x3FB = 0x80`).
- Configures 38,400 baud rate (`DLL = 0x03`, `DLH = 0x00`).
- Sets 8 data bits, no parity, 1 stop bit (8N1) (`0x3FB = 0x03`).
- Enables 14-byte FIFO buffer (`0x3FA = 0xC7`).
- Asserts RTS and DTR lines (`0x3FC = 0x03`).

### 4.4 Dual-Channel Output (`puts`)
Every character of the boot banner is dispatched both to the video display via BIOS teletype `INT 10h, AH=0Eh` and to the COM1 serial transmit holding register (`out 0x3F8, al`), ensuring logs are observable both visually and via automated test pipes.

---

## 5. Transition to Stage 2 and Protected Mode (Phase 2 Preview)

The Stage 1 MBR is constrained to 512 bytes (446 bytes executable code, 64 bytes partition table, 2 bytes signature `0xAA55`). To boot a C kernel, Stage 1 will transition to Stage 2:

1. **BIOS INT 13h Disk Load:**
   - Set `AH = 0x02` (Read Sectors).
   - Set `AL = number of sectors to read`.
   - Set `CH = 0` (Cylinder), `CL = 2` (Sector 2, immediately following MBR).
   - Set `DH = 0` (Head), `DL = [boot_drive]`.
   - Set `ES:BX` to target memory address (e.g. `0x0000:0x7E00` or `0x0000:0x1000`).
   - Call `INT 0x13`, checking Carry Flag (`JC disk_error`).

2. **A20 Line Gate Activation:**
   - Real mode addresses wrap at 1 MB without A20 enabled. A20 must be asserted using the keyboard controller (I/O port `0x64`) or Fast A20 gate (port `0x92`) to address up to 4 GB of memory.

3. **Global Descriptor Table (GDT) Installation:**
   - Define a 32-bit GDT with:
     - Null Descriptor (mandatory 8 zero bytes).
     - Kernel 32-bit Code Segment Descriptor (Base 0, Limit 4GB, Type 0x9A, Flags 0xCF).
     - Kernel 32-bit Data Segment Descriptor (Base 0, Limit 4GB, Type 0x92, Flags 0xCF).
   - Load GDT pointer into CPU with `lgdt [gdt_descriptor]`.

4. **Protected Mode Switch:**
   - Disable interrupts: `cli`.
   - Set Bit 0 (Protection Enable) of Control Register 0:
     ```nasm
     mov eax, cr0
     or eax, 1
     mov cr0, eax
     ```
   - Execute a far jump to the 32-bit code segment to flush the 16-bit pipeline:
     ```nasm
     jmp 0x08:protected_mode_entry
     ```
   - In 32-bit mode, update data segment registers (`DS`, `ES`, `FS`, `GS`, `SS`) to point to the 32-bit data segment selector (`0x10`).
   - Transfer execution to the freestanding C kernel entry point (`kmain`).
