# Eran OS

> A custom operating system engineered from bare-metal x86 assembly to a hardware-aware kernel, modern desktop, and integrated offline AI productivity assistant.

---

## Current Status: Phase 1 — Master Boot Sector (Complete)

Eran OS has successfully completed **Phase 0 (Repository & Build Audit)** and **Phase 1 (Stage 1 Bootloader Stabilization)**.

- **Target Architecture:** x86 32-bit (initially booting in 16-bit real mode via legacy BIOS).
- **Execution Origin:** Physical address `0x7C00`.
- **Boot Format:** Standard 512-byte Master Boot Record (MBR) on a 1.44 MB floppy disk image, verified with boot signature `0xAA55`.
- **Active Subsystems:**
  - Segment normalization (`CS:0000`, `DS:0000`, `ES:0000`, `SS:0000`).
  - Stack initialization (`SP:7C00`, growing downward safely below the boot sector).
  - Direction flag clearing (`CLD`).
  - Boot drive ID preservation (`DL` stored to memory for Stage 2 disk loading).
  - BIOS teletype video output (`INT 10h`, `AH=0Eh`).
  - 16550 UART COM1 serial console initialized (`0x3F8`, 38400 baud, 8N1).
  - Dual-output console (`puts` transmits simultaneously to screen and serial port).
  - Headless automated regression testing in QEMU.

---

## Directory Layout

```
eran-os/
├── Makefile                # Master build system with clean, run, run-cli, debug, and test targets
├── README.md               # Project overview and quick start guide
├── .gitignore              # Ignores build artifacts
├── src/
│   └── main.asm            # Stage 1 MBR bootloader source
├── build/                  # Generated binaries and disk images (created during build)
│   ├── main.bin            # Assembled 512-byte boot sector binary
│   └── main_floppy.img     # 1.44 MB floppy disk image
├── tools/
│   └── test_boot.py        # Automated headless QEMU boot test runner
└── docs/
    ├── architecture.md     # Architectural overview and subsystem evolution
    ├── boot-process.md     # Boot sequence, real mode invariants, and protected mode transition
    ├── roadmap.md          # Multi-phase master implementation roadmap
    ├── hardware-compatibility.md # Hardware compatibility matrix
    └── ai-architecture.md  # Offline AI assistant and AI Shell design specification
```

---

## Getting Started

### Prerequisites

On Ubuntu Linux (or Debian-based distributions), install the necessary build tools and emulator:

```bash
sudo apt update
sudo apt install -y nasm qemu-system-x86 build-essential python3
```

### Building the System

To assemble the bootloader and construct the 1.44 MB floppy image:

```bash
make
```

### Running in QEMU

- **Graphical Mode (VGA Window):**
  ```bash
  make run
  ```

- **Headless CLI Mode (COM1 Serial on stdio):**
  ```bash
  make run-cli
  ```

- **Debugging Mode (QEMU pauses, waiting for GDB on `localhost:1234`):**
  ```bash
  make debug
  ```

### Running Automated Tests

Eran OS includes an automated regression test suite that boots the disk image in a headless QEMU instance, validates binary geometry and MBR signature, and asserts expected serial transmission:

```bash
make test
```

---

## Next Milestone: Phase 2 — Bootloader & Processor-Mode Foundations

The immediate next objective is implementing:
1. BIOS `INT 13h` disk read routines to load Stage 2 / kernel sectors from disk into memory.
2. Enabling the A20 line to access memory beyond 1 MB.
3. Defining the Global Descriptor Table (GDT) with flat 4GB code and data segments.
4. Switching the CPU to 32-bit Protected Mode (`CR0.PE = 1`).
5. Transferring control to 32-bit execution context.
