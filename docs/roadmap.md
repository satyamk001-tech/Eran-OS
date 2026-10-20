# Eran OS Master Implementation Roadmap

This document outlines the multi-phase engineering roadmap for Eran OS, advancing from the initial bare-metal bootloader to a complete, hardware-aware operating system with an offline AI assistant.

---

## Roadmap Overview

| Phase | Subsystem / Focus | Status | Primary Deliverables |
|:-----:|:-------------------|:------:|:---------------------|
| **0** | Repository Audit & Baseline | **COMPLETED** | Initial repo inspection, git initialization, build diagnosis |
| **1** | Working Bootloader | **COMPLETED** | Segment normalization, stack setup, CLD, COM1 serial, automated tests |
| **2** | Stage 2 & Protected Mode | *NEXT* | INT 13h disk loading, A20 gate, 32-bit GDT, CR0.PE switch |
| **3** | Freestanding C Kernel | Planned | Cross-compilation, linker script, `kmain`, VGA text driver |
| **4** | CPU Exceptions & Interrupts | Planned | IDT, PIC 8259 remap, CPU exception handlers, PIT timer |
| **5** | Memory Management | Planned | Physical Page Allocator (PMM), Virtual Memory (VMM/Paging), Heap |
| **6** | Initial Input & Console | Planned | PS/2 keyboard driver, scancode decoder, interactive CLI |
| **7** | HAL & Device Management | Planned | Port/MMIO abstraction, PCI enumeration, driver registration |
| **8** | Storage & Filesystem | Planned | ATA/IDE disk driver, Virtual Filesystem (VFS), FAT12/16 read/write |
| **9** | USB Subsystem Foundation | Planned | Host controller abstraction (UHCI/EHCI), USB HID keyboard/mouse |
| **10** | Multitasking & User Mode | Planned | Process Control Blocks (PCB), round-robin scheduler, syscalls, Ring 3 |
| **11** | Networking Stack | Planned | e1000/RTL8139 NIC drivers, Ethernet framing, ARP, IPv4, ICMP, UDP/TCP |
| **12** | Audio & Advanced Input | Planned | Intel HDA / AC97 drivers, audio buffers, volume control |
| **13** | Graphics & Desktop UI | Planned | VBE linear framebuffer, window manager, font engine, desktop shell |
| **14** | Laptop & Power Support | Planned | ACPI table parsing, battery reporting, thermal monitoring, shutdown |
| **15** | AI Shell Prototype | Planned | Host-side Python daemon, structured schemas, tool executor, fallback |
| **16** | Offline AI Integration | Planned | Local quantization runtime (llama.cpp), document & query assistant |
| **17** | Voice Assistant | Planned | Offline speech-to-text (Whisper/Vosk), text-to-speech synthesis |
| **18** | OS-Service Integration | Planned | Native C/C++ AI daemon, safe system service APIs, permission prompts |
| **19** | Security & Hardening | Planned | Capability enforcement, path sandboxing, resource quotas, audit logging |
| **20** | Hardware Validation | Planned | Bare-metal PC testing, UEFI & x86_64 architecture ports |

---

## Phase Details and Criteria

### Phase 0: Repository Audit & Baseline
- **Completed:** 2026-10-20
- **Criteria Met:**
  - Full inspection of workspace files and directory tree.
  - Initialized git tracking baseline commit (`f0460eb`).
  - Audited toolchain (`nasm`, `gcc`, `qemu-system-i386`, `make`, `python3`).
  - Identified missing build rules, missing clean targets, unhandled segment registers, and lack of serial debugging.

### Phase 1: Working Bootloader Stabilization
- **Completed:** 2026-10-20
- **Criteria Met:**
  - Far jump CS normalization (`jmp 0x0000:.init_cs`).
  - Segments reset to 0x0000; stack pointer set at `0x7C00`.
  - Direction Flag cleared (`CLD`) before string operations.
  - BIOS boot drive number (`DL`) preserved for subsequent disk loads.
  - COM1 UART 16550 serial console initialized (`0x3F8`, 38400 baud, 8N1).
  - Dual-output teletype (`puts` writes to BIOS INT 10h and COM1).
  - Master `Makefile` created with `all`, `clean`, `run`, `run-cli`, `debug`, and `test` targets.
  - Automated test suite (`tools/test_boot.py`) verifying binary geometry, MBR signature, and serial strings in headless QEMU.

### Phase 2: Stage 2 Loader & Protected Mode Transition (Next Milestone)
- **Target Deliverables:**
  - Implement Stage 2 disk reading in real mode via BIOS `INT 13h, AH=02h`.
  - Handle disk errors with descriptive reporting to screen and serial port.
  - Enable A20 line to access memory above 1 MB.
  - Define and load 32-bit Global Descriptor Table (GDT).
  - Enter 32-bit Protected Mode (`CR0.PE = 1`) and execute 32-bit far jump.
  - Verify 32-bit execution by writing directly to VGA video memory at `0xB8000`.

### Phase 3: Freestanding C Kernel
- **Target Deliverables:**
  - Linker script (`linker.ld`) placing kernel at `0x100000` (1 MB) or appropriate offset.
  - 32-bit entry stub (`entry.asm`) calling freestanding C `kmain()`.
  - C freestanding runtime headers (`stdint.h`, `stddef.h`, `stdbool.h`).
  - VGA text-mode driver with scrolling, color attributes, and formatted printing (`kprintf`).
