# Eran OS Hardware Compatibility Matrix

This document tracks verified hardware, virtual controllers, and driver implementations in Eran OS.

---

## Hardware Support Status

| Category | Device / Model | Controller / Protocol | Status | Verified Functionality | Limitations | Test Environment |
|:---|:---|:---|:---:|:---|:---|:---|
| **CPU** | x86 (IA-32) Compatible | Real Mode (16-bit) | **Functionality verified** | Segment setup, stack setup, CLD, far jump CS normalize | Handover to Protected Mode | QEMU / VirtualBox |
| **CPU** | x86 (IA-32) Compatible | Protected Mode (32-bit) | **Functionality verified** | GDT flat 4GB segments, CR0.PE switch, 32-bit execution at 0x10000 | Multi-core SMP planned | QEMU i386 |
| **CPU Diagnostics** | CPUID Instruction | Extended Functions | **Functionality verified** | Vendor string, family/model/stepping, SSE/AVX flags, 64-bit Long Mode check | None | QEMU i386 |
| **Paging / VMM** | Two-Level Paging | CR0.PG / CR3 Page Tables | **Functionality verified** | 1024-entry Page Directory, 4 Page Tables, 16 MB identity mapping, Page Fault handler (IDT 14) | Dynamic swapping planned | QEMU i386 |
| **Interrupts** | Intel 8259 PIC | Master (0x20) / Slave (0xA0) | **Functionality verified** | IRQs 0-15 remapped to 32-47, selective unmasking, EOI dispatch | IO-APIC planned | QEMU i386 |
| **Exceptions** | x86 CPU Exceptions | Vectors 0–31 | **Functionality verified** | Complete IDT gates, panic reports with register dumps & EIP | None | QEMU i386 |
| **Timer** | Intel 8254 PIT | I/O Ports 0x40 / 0x43 | **Functionality verified** | IRQ 0 @ 100 Hz, uptime tracking in seconds/ticks, calibrated sleep (`sleep_ticks`, `sleep_ms`) | HPET planned | QEMU i386 |
| **Keyboard** | 8042 PS/2 Controller | Port 0x60 / IRQ 1 | **Functionality verified** | Scancode set 1 translation, Shift/Caps Lock, ring buffer, interactive shell | USB HID keyboard driver | QEMU PS/2 |
| **Display** | VGA Direct Memory | MMIO `0xB8000` | **Functionality verified** | 80x25 text buffer, color attributes, auto-scrolling, CRT cursor I/O (`0x3D4/0x3D5`) | VBE high-res linear framebuffer | QEMU Standard VGA |
| **Graphics UI** | 2D Canvas & Desktop | Software Framebuffer | **Functionality verified** | Double-buffered 2D canvas, draggable window structures, system taskbar, window switcher | Hardware 3D acceleration | QEMU Standard VGA |
| **Serial** | 16550 UART (COM1) | Port I/O `0x3F8` | **Functionality verified** | 38400 baud 8N1, TX polling transmit, RX buffer polling, dual-channel console | FIFO IRQ trigger | QEMU `-serial stdio` |
| **Bus** | PCI Configuration Space | Ports `0xCF8` / `0xCFC` | **Functionality verified** | Bus scanning, device/vendor ID reading, class decoding, hardware list | Device configuration writes | QEMU PCI Bus |
| **Storage (Floppy)** | 3.5" 1.44MB Floppy | BIOS INT 13h / Diskette | **Functionality verified** | Sector loop loader reading 180 sectors (90 KB) to physical RAM `0x10000` | Read-only boot | QEMU Floppy |
| **Storage (ATA)** | Parallel ATA / IDE | Primary Port 0x1F0 | **Functionality verified** | PIO mode sector read/write, drive identification, status polling | DMA bus mastering planned | QEMU Primary IDE |
| **Network (NIC)** | Intel 82540EM (e1000) | PCI Gigabit Controller | **Functionality verified** | MAC address reading, packet transmission, receive queue polling, packet metrics | Interrupt-driven rings | QEMU `-net nic,model=e1000` |
| **Network (Stack)** | IPv4 / ARP / ICMP | L2/L3 Packet Stack | **Functionality verified** | Static IP (`192.168.1.50`), subnet, gateway, ICMP Echo ping client (`ping 192.168.1.1`) | TCP stream reassembly planned | QEMU User Network |
| **Audio** | PC Speaker Sound | 8254 PIT Ch 2 + Port 0x61 | **Functionality verified** | Square-wave tone synthesis, 880 Hz system alert `beep`, 4-note melodic `chime` | AC97 / Intel HDA codecs | QEMU PC Speaker |
| **Power & Battery**| ACPI 1.0/2.0 | BIOS ROM (0xE0000-0xFFFFF)| **Functionality verified** | RSDP pointer validation, RSDT/FADT table parsing, ACPI soft power-off, battery charge telemetry | Thermal fan curve control | QEMU ACPI / BOCHS |
| **USB Subsystem** | UHCI / EHCI USB Host | PCI Class 0x0C / Subclass 03 | **Functionality verified** | Controller discovery, root hub enumeration, USB HID & mass storage device structures | xHCI USB 3.0 controller | QEMU USB |
| **Memory** | Physical RAM Frames | Bitmap Page Allocator | **Functionality verified** | 4KB physical page allocation/freeing, 32MB RAM, frame allocation tracking | Physical memory hotplug | QEMU (32MB RAM) |
| **Heap** | Kernel Dynamic Memory | Boundary-Tag Allocator | **Functionality verified** | `kmalloc`, `kfree`, block splitting, coalescing, dynamic statistics across 4MB | None | QEMU (4MB Heap) |
| **Filesystem**| Virtual Filesystem (VFS) | In-Memory RamFS | **Functionality verified** | Directory listing (`ls`), file content reading (`cat`), system text files | Persistent FAT32/ext2 write | QEMU |
| **AI Assistant (Native)**| Native C AI Core | Ring 3 Unprivileged / Syscall | **Functionality verified** | Natural language queries inside kernel, memory telemetry, task info, audio playback | Large neural weight matrix | Eran Kernel Shell |
| **AI Assistant (Host)**| Eran AI Shell | Python Host Ring 3 Sandbox | **Functionality verified** | Intent parsing, structured plans, path traversal block (`../`), safe execution | None | Linux Host / Python 3 |

---

## Verified Test Configuration
- **Hypervisor / Emulator:** QEMU 8.2.2 (`qemu-system-i386`).
- **Network Device:** Intel 82540EM (`-net nic,model=e1000 -net user`).
- **Assembler:** NASM 2.16.01.
- **Compiler:** GCC 13.3.0 (`-m32 -ffreestanding -O2 -Wall -Wextra`).
- **Linker:** GNU Binutils LD 2.42 (`-m elf_i386`).
- **Automated Verification:** `tools/test_boot.py` (OS regression) & `tools/test_ai.py` (AI Shell & security tests).
