# ERAN OS — Hardware & Subsystem Verification Guide

This document describes the automated test suite, headless QEMU verification harness, and manual hardware invariant checks implemented for **ERAN OS**.

---

## 1. Automated Bare-Metal Verification (`tools/test_kernel.py`)

The kernel test suite runs inside a headless QEMU instance (`qemu-system-i386`), redirecting COM1 UART 16550 serial output directly to standard output. An automated Python assertion harness validates the initialization state of all core architectural subsystems.

### Executing the Test Suite

```bash
# 1. Compile the bootloader and kernel disk image
make clean
make all

# 2. Run the automated hardware subsystem verification harness
python3 tools/test_kernel.py
```

### Verified Subsystem Invariants

| Subsystem Test | Expected Hardware Invariant | Pass Criterion |
|---|---|---|
| **Bootloader & Real Mode** | Stage 1 MBR loads kernel to `0x10000` | Log: `Target: x86 32-bit Protected Mode (Base: 0x10000)` |
| **GDT & TSS** | Ring 0/Ring 3 Segments + TSS `0x28` loaded | Log: `GDT active (Kernel/User Ring 0/3 segments + TSS 0x28)` |
| **IDT & PIC** | Remap master/slave PIC (0x20-0x2F), 32 CPU traps | Log: `IDT active (32 CPU Exceptions + 16 IRQs remapped)` |
| **PIT Timer & Keyboard** | 8254 Timer at 100 Hz, PS/2 Keyboard IRQ 1 | Log: `Initializing 8254 PIT Timer (100 Hz)` |
| **Physical Memory (PMM)** | 32 MB RAM bitmap allocator tracking 8,192 frames | Log: `PMM initialized` |
| **Kernel Heap Allocator** | 4 MB contiguous heap pool initialized at `0x200000` | Log: `Kernel Heap initialized` |
| **Virtual Memory (VMM)** | Two-level paging active (`CR0.PG = 1`), 16MB identity | Log: `VMM Paging active (CR0.PG=1, lower 16MB identity mapped)` |
| **Syscall Gate** | Software Interrupt `0x80` dispatcher online | Log: `System call dispatcher active` |
| **Multitasking Scheduler** | Round-robin scheduler active with PID 0 and PID 1 | Log: `Multitasking scheduler active` |
| **Virtual Filesystem** | In-memory RamFS mounted at root `/` | Log: `VFS root '/' mounted` |

---

## 2. Interactive Testing in QEMU

To run ERAN OS with full graphical VGA display, keyboard, mouse, and networking:

```bash
# Build and boot directly into QEMU with VGA display
make run

# Boot in CLI text mode with serial logging
make run-cli

# Run with GDB remote debugger attached (port 1234)
make debug
```

---

## 3. Subsystem Implementation Classification

| Subsystem | Classification | Implementation Notes |
|---|---|---|
| Stage 1 Boot Sector | **WORKING** | 512-byte MBR, real-to-protected mode switch, A20 line enable, INT 13h kernel loading |
| Global Descriptor Table (GDT) | **WORKING** | Kernel/User Code/Data segments, TSS segment selector `0x28` |
| Interrupt System (IDT/PIC) | **WORKING** | 8259 PIC remapped to vectors 0x20-0x2F; 32 CPU exception ISR stubs |
| Programmable Interval Timer (PIT)| **WORKING** | 8254 Channel 0 configured for 100 Hz timer tick frequency |
| Physical Memory Manager (PMM) | **WORKING** | Bitmap page frame allocator (4 KB pages) managing 32 MB RAM |
| Kernel Heap Allocator | **WORKING** | 4 MB heap pool with dynamic `kmalloc` / `kfree` |
| Virtual Memory Manager (VMM) | **WORKING** | 2-level x86 paging (CR3, Page Directory, Page Tables, CR0.PG=1) |
| System Call Interface | **WORKING** | INT 0x80 gate with register-based ABI (EAX, EBX, ECX, EDX) |
| Process Management & Scheduler | **WORKING** | Cooperative & timer-preemptive round-robin scheduler (`pcb_t`) |
| Virtual Filesystem (VFS) | **WORKING** | Hierarchical in-memory RamFS (`/bin/hello.elf`, `/etc/version`) |
| IDE / Parallel ATA Driver | **PARTIALLY WORKING** | PIO Mode sector read/write on ports 0x1F0-0x1F7 |
| PCI Bus Enumerator | **WORKING** | I/O port configuration space scanning (`0xCF8`/`0xCFC`) |
| Intel e1000 Gigabit NIC | **PARTIALLY WORKING** | PCI bar discovery, ring descriptor configuration, frame TX/RX |
| Network Protocol Stack | **PARTIALLY WORKING** | Ethernet II frame parser, ARP table, IPv4, ICMP Echo |
| TCP Stream Manager | **EXPERIMENTAL** | Minimal RFC 793 connection state machine |
| Audio Subsystem | **WORKING** | PC Speaker square wave generator using PIT Channel 2 (`0x42`) |
| PS/2 Keyboard & Mouse | **WORKING** | 8042 scancode decoder and auxiliary packet streaming |
| ACPI Power Management | **PARTIALLY WORKING** | RSDP table pointer discovery and checksum validation |
| Interactive Shell & Utilities | **WORKING** | 35+ commands (`help`, `ls`, `cat`, `mem`, `pci`, `ping`, `desktop`, `run`) |
| ELF Binary Loader | **PARTIALLY WORKING** | ELF32 header verification, section loading, Ring 3 jump |
