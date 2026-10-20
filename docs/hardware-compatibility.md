# Eran OS Hardware Compatibility Matrix

This matrix documents the verification and support status of physical and virtual hardware targets in Eran OS.

Statuses:
- **Planned:** Architecture mapped; no code written yet.
- **Not implemented:** Component identified but not active.
- **Detected only:** Hardware identifier probed; driver not initialized.
- **Driver in development:** Driver logic partially implemented.
- **Driver initialized:** Device configures without hardware errors.
- **Partially tested:** Basic I/O functions; edge cases unverified.
- **Functionality verified:** Full automated test suite passes on target.

---

## Hardware Support Status

| Category | Device / Model | Controller / Protocol | Status | Verified Functionality | Limitations | Test Environment |
|:---|:---|:---|:---:|:---|:---|:---|
| **CPU** | x86 (IA-32) Compatible | Real Mode (16-bit) | **Functionality verified** | Segment setup, stack setup, CLD, far jump CS normalize | Protected Mode not yet entered | QEMU i386 / VirtualBox |
| **CPU** | x86 (IA-32) Compatible | Protected Mode (32-bit) | *Planned* | None | Target for Phase 2 | QEMU i386 |
| **Serial** | 16550 UART (COM1) | Port I/O `0x3F8` | **Functionality verified** | Initialization (38400 8N1), polling TX character transmit | Interrupt-driven RX not yet enabled | QEMU `-serial stdio` |
| **Display** | VGA Standard Text | BIOS INT 10h Teletype | **Functionality verified** | Real mode character printing via AH=0Eh | 80x25 text mode only; no direct VRAM writes yet | QEMU Standard VGA |
| **Display** | VGA Direct Memory | `0xB8000` MMIO | *Planned* | None | Planned for Phase 2/3 | QEMU Standard VGA |
| **Display** | VBE Framebuffer | VESA 2.0+ LFB | *Planned* | None | Requires graphical desktop (Phase 13) | QEMU / Physical GPU |
| **Storage** | 3.5" 1.44MB Floppy | BIOS INT 13h / Controller | **Partially tested** | MBR loaded and executed at `0x7C00` | Subsequent sector reading not yet implemented | QEMU `-drive if=floppy` |
| **Storage** | Parallel ATA / IDE | Port I/O `0x1F0` / PIO | *Planned* | None | Planned for Phase 8 | QEMU IDE controller |
| **Storage** | Serial ATA (AHCI) | PCI MMIO (AHCI 1.0+) | *Planned* | None | Requires PCI enumeration (Phase 8) | QEMU `ich9-ahci` |
| **Input** | PS/2 Keyboard | Port I/O `0x60`, `0x64` | *Planned* | None | Planned for Phase 6 | QEMU PS/2 |
| **Input** | PS/2 Mouse | Port I/O `0x60`, `0x64` | *Planned* | None | Planned for Phase 6 | QEMU PS/2 Mouse |
| **USB** | UHCI / EHCI Host | PCI Controller | *Planned* | None | Planned for Phase 9 | QEMU USB |
| **USB** | USB HID Keyboard/Mouse | USB Class 03h | *Planned* | None | Planned for Phase 9 | QEMU USB HID |
| **Audio** | Intel High Definition Audio | PCI MMIO / Codec | *Planned* | None | Planned for Phase 12 | QEMU `hda` |
| **Network**| Intel 82540EM (e1000) | PCI MMIO / DMA | *Planned* | None | Planned for Phase 11 | QEMU `e1000` |
| **Power**  | ACPI PM Timer & DSDT | ACPI Table Parser | *Planned* | None | Planned for Phase 14 | QEMU ACPI |

---

## Verified Test Configuration
- **Host OS:** Ubuntu Linux running inside VirtualBox.
- **Hypervisor / Emulator:** QEMU 8.2.2 (`qemu-system-i386`).
- **Assembler:** NASM 2.16.01.
- **Build Tool:** GNU Make 4.3.
- **Test Automation:** Python 3.12 (`tools/test_boot.py`).
