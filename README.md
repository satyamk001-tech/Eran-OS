# Eran OS

> A custom operating system engineered from bare-metal x86 assembly to a hardware-aware kernel, modern desktop, and integrated offline AI productivity assistant.

---

## Current Status: Kernel v0.2.0 (Milestone Complete)

Eran OS has successfully advanced through the foundational bootloader, processor mode switch, freestanding 32-bit C & Assembly kernel, low-level hardware abstraction, memory managers, PCI discovery, Virtual Filesystem, interactive OS shell, and unprivileged host-side Offline AI Assistant.

### Fully Implemented & Verified Subsystems:
1. **Stage 1 & 2 Bootloader (`boot/boot.asm`):**
   - 16-bit Real Mode MBR loaded at `0x7C00`.
   - Multi-track / multi-head floppy disk loading via BIOS `INT 13h` (loads 53 sectors = 27 KB into memory at physical `0x10000`).
   - Fast A20 gate activation (port `0x92`).
   - 32-bit Global Descriptor Table (GDT) configuration (Flat 4 GB Code & Data segments).
   - Control Register 0 Protection Enable (`CR0.PE = 1`) transition with far jump pipeline flush.
   - 32-bit stack setup at `0x90000` and execution handover to kernel at `0x10000`.
2. **Freestanding 32-bit C & Assembly Kernel (`kernel/`):**
   - 32-bit entry stub (`kernel/entry.asm`).
   - Custom Linker Script (`kernel/linker.ld`) linking code, rodata, data, and bss.
   - Core dispatcher (`kernel/kernel.c`).
3. **Interrupt & Exception Subsystem (`kernel/idt.c`, `kernel/isr.asm`, `kernel/pic.c`):**
   - 256-entry Interrupt Descriptor Table (IDT).
   - 32 CPU Exception handlers with register dumps and fault reporting.
   - 8259 Programmable Interrupt Controller (PIC) remapped (IRQs 0-15 to vectors 32-47).
   - Global hardware interrupt management (`sti`/`cli`).
4. **Device Drivers & HAL (`drivers/`, `hal/`):**
   - **Display:** Direct VGA Text Memory MMIO (`0xB8000`) driver with color attributes, auto-scrolling, CRT cursor updates, and formatted `kprintf`.
   - **Serial Console:** 16550 UART COM1 driver (`0x3F8`, 38400 baud, 8N1) for automated testing.
   - **Hardware Timer:** 8254 Programmable Interval Timer (PIT) on IRQ 0 configured for 100 Hz (10 ms ticks), uptime tracking, and calibrated sleep.
   - **Keyboard:** Standard 8042 PS/2 Keyboard driver on IRQ 1 (port `0x60`), scancode set 1 decoder, Shift and Caps Lock support, and circular keystroke buffer.
   - **PCI Enumeration:** HAL PCI configuration reader (ports `0xCF8`/`0xCFC`) scanning buses and discovering peripheral controllers (VGA, IDE/SATA, Network, Host Bridges).
5. **Memory Management (`mm/`):**
   - **Physical Memory Manager (PMM):** Bitmap frame allocator managing 4KB physical pages, protecting low memory and kernel image.
   - **Kernel Dynamic Heap:** Linked-list boundary-tag allocator (`kmalloc`, `kfree`) providing dynamic kernel allocation.
6. **Virtual Filesystem & RamFS (`fs/vfs.c`):**
   - VFS node hierarchy supporting directory browsing and file reading.
   - In-memory RamFS populated with `/README.txt`, `/system/os_info.txt`, `/etc/hostname`, and `/devices.txt`.
7. **Interactive Kernel Shell (`kernel/shell.c`):**
   - Interactive command console with line editing and commands:
     `help`, `version`, `clear`, `cpuinfo`, `meminfo`, `uptime`, `devices`, `ls`, `cat`, `echo`, `reboot`, `shutdown`, `panic`.
8. **Offline AI Assistant & Shell (`ai-shell/`):**
   - Ring 3 unprivileged architecture running separately from kernel space.
   - Natural language intent classification (file discovery, hardware query, system status, doc reading).
   - Structured action planning (schemas, no arbitrary code execution).
   - Security Policy Engine blocking directory traversal (`../`) and enforcing workspace boundaries.
   - Sandboxed task executor and deterministic offline provider.

---

## Directory Layout

```
eran-os/
├── Makefile                # Master build system
├── README.md               # Project documentation and user guide
├── .gitignore              # Ignores build artifacts
├── boot/
│   └── boot.asm            # Stage 1 MBR & Stage 2 protected mode switch
├── kernel/
│   ├── entry.asm           # 32-bit kernel entry point
│   ├── kernel.c            # kmain entry, banner, subsystem init
│   ├── isr.asm             # CPU exception and IRQ assembly stubs
│   ├── idt.h / idt.c       # IDT interrupt descriptor table
│   ├── pic.h / pic.c       # 8259 PIC remapping and EOI
│   ├── shell.h / shell.c   # Interactive kernel shell
│   └── linker.ld           # Kernel linker script (0x10000 physical)
├── hal/
│   ├── io.h                # In/out port I/O primitives
│   └── pci.h / pci.c       # PCI bus scanner and hardware discovery
├── drivers/
│   ├── vga.h / vga.c       # Direct VGA text MMIO (0xB8000) & kprintf
│   ├── serial.h / serial.c # 16550 UART serial driver (COM1 0x3F8)
│   ├── timer.h / timer.c   # PIT 8254 timer (100 Hz, IRQ 0)
│   └── keyboard.h / keyboard.c # PS/2 keyboard driver (IRQ 1)
├── mm/
│   ├── pmm.h / pmm.c       # Bitmap physical page frame allocator
│   └── heap.h / heap.c     # Dynamic kernel heap (kmalloc / kfree)
├── fs/
│   └── vfs.h / vfs.c       # Virtual Filesystem & RamFS mount
├── tools/
│   ├── test_boot.py        # Automated headless QEMU kernel regression test
│   └── test_ai.py          # AI Shell test suite runner
├── ai-shell/
│   ├── pyproject.toml
│   ├── src/eran_shell/
│   │   ├── main.py         # AI Shell CLI and interactive REPL
│   │   ├── interface.py    # Formatting and terminal UI
│   │   ├── intent.py       # Intent classification
│   │   ├── planner.py      # Structured action schemas
│   │   ├── policy.py       # Security boundary & path traversal sandbox
│   │   ├── executor.py     # Safe action executor
│   │   └── providers/      # Modular offline NLP & LLM providers
│   └── tests/
│       └── test_ai_shell.py # Security & unit tests
└── docs/                   # Architecture, boot process, roadmap, hardware matrix, AI spec
```

---

## Build and Run Instructions

### 1. Build Operating System Image
```bash
make clean && make
```

### 2. Run Eran OS in QEMU
- **Interactive Graphical Window (VGA + PS/2 Keyboard):**
  ```bash
  make run
  ```
- **Headless Mode (Serial COM1 on stdio):**
  ```bash
  make run-cli
  ```
- **Debug Mode (GDB on `localhost:1234`):**
  ```bash
  make debug
  ```

### 3. Run Automated Regression Test Suites
- **Operating System & Kernel Tests:**
  ```bash
  make test
  ```
- **Offline AI Shell & Security Policy Tests:**
  ```bash
  make test-ai
  ```

### 4. Run Interactive Offline AI Shell
```bash
make run-ai
```
*(Or single query: `python3 ai-shell/src/eran_shell/main.py -c "show connected devices"`)*
