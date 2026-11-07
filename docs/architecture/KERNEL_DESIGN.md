# ERAN OS — Execution and Resource Administration Nucleus
## Kernel Architecture & Subsystem Specification

---

## 1. Architectural Philosophy & Overview

**ERAN OS** is an independent, freestanding 32-bit x86 monolithic kernel and bootloader written from scratch in Assembly (NASM) and Freestanding C (`-nostdlib -ffreestanding -O2`). It features hardware-enforced memory safety, two-level paging, multitasking process management, preemptive PIT timer scheduling, dynamic kernel heap allocation, a Virtual Filesystem (RamFS), device drivers (VGA, PS/2 Keyboard/Mouse, 16550 UART, PCI, Intel e1000 Gigabit NIC), an INT 0x80 syscall ABI, and Ring 3 user privilege isolation.

---

## 2. Boot Sequence & Hardware Initialization

```
+-------------------------------------------------------------+
| BIOS Stage                                                  |
| 1. Power-On Self Test (POST)                                |
| 2. BIOS loads 512-byte MBR from sector 0 to address 0x7C00  |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
| Stage 1 Bootloader (boot/boot.asm - Real Mode 16-bit)       |
| 1. Set DS=ES=SS=0x0000, SP=0x7C00                           |
| 2. Enable A20 Line (Fast A20 via system control port 0x92)  |
| 3. Read Kernel sectors (40 sectors = 20KB) via INT 13h, ah=02h|
|    Buffer destination: Physical address 0x10000             |
| 4. Load Global Descriptor Table (lgdt [gdt_descriptor])     |
| 5. Switch to 32-bit Protected Mode (CR0 |= 0x01)            |
| 6. Far Jump (jmp 0x08:init_pm) -> Flush 16-bit pipeline     |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
| Kernel Entry (kernel/entry.asm - 32-bit Protected Mode)     |
| 1. Reload Segment Registers: DS=ES=FS=GS=SS = 0x10 (Data)   |
| 2. Setup Kernel Stack: ESP = 0x90000                        |
| 3. Jump to High-Level C Kernel Entry Point: call kmain      |
+-------------------------------------------------------------+
```

---

## 3. Physical Memory Map (32 MB Baseline)

| Memory Range | Size | Classification | Purpose |
|---|---|---|---|
| `0x00000000 - 0x000003FF` | 1 KB | Real Mode | Real-mode Interrupt Vector Table (IVT) |
| `0x00000400 - 0x000004FF` | 256 B | Real Mode | BIOS Data Area (BDA) |
| `0x00000500 - 0x00007BFF` | ~30 KB | Free RAM | Scratch / Transient boot memory |
| `0x00007C00 - 0x00007DFF` | 512 B | Bootloader | MBR Stage 1 Boot Sector |
| `0x00010000 - 0x0008FFFF` | 512 KB | Kernel Code | Compiled Kernel Binary (`kernel.bin`) |
| `0x00090000 - 0x0009FFFF` | 64 KB | Kernel Stack | Initial Execution Stack (`0x90000`) |
| `0x000A0000 - 0x000BFFFF` | 128 KB | Memory-Mapped I/O | Video Display RAM (Text Mode `0xB8000`) |
| `0x000C0000 - 0x000FFFFF` | 256 KB | ROM | Video BIOS and Motherboard BIOS |
| `0x00100000 - 0x001FFFFF` | 1 MB | Physical PMM | Physical Memory Bitmap Tracker |
| `0x00200000 - 0x005FFFFF` | 4 MB | Dynamic Heap | Kernel Dynamic Memory Pool (`kmalloc`/`kfree`) |
| `0x00600000 - 0x01FFFFFF` | 26 MB | Page Frames | Dynamic Page Allocator & User Memory Space |

---

## 4. Memory Management Subsystems

### A. Physical Memory Manager (PMM - Bitmap Allocator)
- **Granularity**: 4,096 bytes (4 KB standard x86 page frames)
- **Tracking Structure**: Linear bit array where `1 = Allocated`, `0 = Free`
- **Total Tracked Frames**: 8,192 frames across 32 MB physical RAM
- **Allocation Primitives**:
  - `pmm_alloc_page()`: Scans bitmap using bitwise test-and-set for first free frame ($O(1)$ amortized word search).
  - `pmm_free_page(void* addr)`: Clears corresponding bit index in bitmap.

### B. Virtual Memory Manager (VMM - Two-Level Paging)
- **Directory Layout**: 1,024 Page Directory Entries (PDE), each referencing a Page Table.
- **Table Layout**: 1,024 Page Table Entries (PTE), each mapping a 4 KB physical frame.
- **Hardware Configuration**:
  - `CR3` register loaded with physical address of Page Directory (`0x1000` aligned).
  - Paging activated via Control Register 0: `CR0.PG = 1` (`or eax, 0x80000000`).
- **Initial Mapping**: Lower 16 MB identity mapped (`Virtual 0x00000000 - 0x00FFFFFF` $\rightarrow$ `Physical 0x00000000 - 0x00FFFFFF`) with Supervisor read/write permissions.

### C. Kernel Dynamic Heap (`kmalloc` / `kfree`)
- **Base Address**: `0x00200000` (2 MB physical mark)
- **Heap Size**: 4 MB contiguous pool
- **Algorithm**: Linked-list boundary tag allocator with split and coalesce mechanisms to prevent memory fragmentation.

---

## 5. CPU Privilege Rings & System Call Interface

### A. Global Descriptor Table (GDT)
The kernel initializes 6 hardware descriptors:
1. `0x00`: Null Descriptor (Required by Intel IA-32 specification)
2. `0x08`: Kernel Code Segment (Ring 0, Base 0x0, Limit 4GB, Type 0x9A)
3. `0x10`: Kernel Data Segment (Ring 0, Base 0x0, Limit 4GB, Type 0x92)
4. `0x18`: User Code Segment (Ring 3, Base 0x0, Limit 4GB, Type 0xFA)
5. `0x20`: User Data Segment (Ring 3, Base 0x0, Limit 4GB, Type 0xF2)
6. `0x28`: Task State Segment (TSS, Ring 0 stack pointer `esp0 = 0x90000`, `ss0 = 0x10`)

### B. System Call ABI (Software Interrupt `0x80`)
User-space programs transition to Ring 0 via `int 0x80`:
- **`EAX`**: Syscall Number
  - `0x01` (`SYS_EXIT`): Terminate active process
  - `0x02` (`SYS_FORK`): Fork child process
  - `0x03` (`SYS_READ`): Read data from VFS file descriptor
  - `0x04` (`SYS_WRITE`): Write data to stdout/serial
  - `0x05` (`SYS_YIELD`): Voluntarily yield CPU time slice
  - `0x06` (`SYS_GETPID`): Retrieve current Process ID
  - `0x07` (`SYS_SLEEP`): Sleep for $N$ PIT timer ticks
- **`EBX`**: First Parameter
- **`ECX`**: Second Parameter
- **`EDX`**: Third Parameter
- **`ESI`**: Fourth Parameter
- **Return Value**: Passed back in `EAX` register.
