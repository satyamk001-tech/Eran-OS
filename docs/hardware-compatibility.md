# Eran OS Hardware Compatibility Matrix

This document tracks verified hardware, virtual controllers, and driver implementations in Eran OS.

---

## Hardware Support Status

| Category | Device / Model | Controller / Protocol | Status | Verified Functionality | Limitations | Test Environment |
|:---|:---|:---|:---:|:---|:---|:---|
| **CPU** | x86 (IA-32) Compatible | Real Mode (16-bit) | **Functionality verified** | Segment setup, stack setup, CLD, far jump CS normalize | Handover to Protected Mode | QEMU / VirtualBox |
| **CPU** | x86 (IA-32) Compatible | Protected Mode (32-bit) | **Functionality verified** | GDT flat 4GB segments, CR0.PE switch, 32-bit execution at 0x10000 | Paging (VMM) planned | QEMU i386 |
| **Interrupts** | Intel 8259 PIC | Master (0x20) / Slave (0xA0) | **Functionality verified** | IRQs 0-15 remapped to 32-47, selective unmasking, EOI dispatch | APIC planned | QEMU i386 |
| **Exceptions** | x86 CPU Exceptions | Vectors 0–31 | **Functionality verified** | Complete IDT gates, panic reports with register dumps & EIP | None | QEMU i386 |
| **Timer** | Intel 8254 PIT | I/O Ports 0x40 / 0x43 | **Functionality verified** | IRQ 0 @ 100 Hz, uptime tracking in seconds/ticks, calibrated sleep | High-precision HPET planned | QEMU i386 |
| **Keyboard** | 8042 PS/2 Controller | Port 0x60 / IRQ 1 | **Functionality verified** | Scancode set 1 translation, Shift/Caps Lock, ring buffer, interactive shell | USB HID keyboard planned | QEMU PS/2 |
| **Display** | VGA Direct Memory | MMIO `0xB8000` | **Functionality verified** | 80x25 text buffer, color attributes, auto-scrolling, CRT cursor I/O | VBE graphics planned | QEMU Standard VGA |
| **Serial** | 16550 UART (COM1) | Port I/O `0x3F8` | **Functionality verified** | 38400 baud 8N1, TX polling transmit, dual-channel console logging | Polling TX; IRQ RX planned | QEMU `-serial stdio` |
| **Bus** | PCI Configuration Space | Ports `0xCF8` / `0xCFC` | **Functionality verified** | Bus scanning, device/vendor ID reading, class decoding, hardware list | Device configuration writes | QEMU PCI Bus |
| **Storage** | 3.5" 1.44MB Floppy | BIOS INT 13h / Diskette | **Functionality verified** | Multi-track / multi-head sector reading (53 sectors loaded to RAM) | ATA/IDE driver planned | QEMU Floppy |
| **Memory** | Physical RAM Frames | Bitmap Page Allocator | **Functionality verified** | 4KB physical page allocation/freeing, low-RAM reservation (2MB) | Paging tables planned | QEMU (32MB RAM) |
| **Heap** | Kernel Dynamic Memory | Linked-List Heap | **Functionality verified** | `kmalloc`, `kfree`, block splitting, coalescing, dynamic statistics | None | QEMU (4MB Heap) |
| **Filesystem**| Virtual Filesystem (VFS) | In-Memory RamFS | **Functionality verified** | Directory listing (`ls`), file content reading (`cat`), VFS nodes | Persistent disk write planned | QEMU |
| **AI Assistant**| Eran AI Shell | Python Host Ring 3 Sandbox | **Functionality verified** | Intent parsing, structured plans, path traversal block, safe execution | Native C port planned | Linux Host / Python 3 |

---

## Verified Test Configuration
- **Hypervisor / Emulator:** QEMU 8.2.2 (`qemu-system-i386`).
- **Assembler:** NASM 2.16.01.
- **Compiler:** GCC 13.3.0 (`-m32 -ffreestanding -O2`).
- **Linker:** GNU Binutils LD 2.42 (`-m elf_i386`).
- **Automated Verification:** `tools/test_boot.py` (OS regression) & `tools/test_ai.py` (AI Shell & security tests).
