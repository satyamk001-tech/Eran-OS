#!/usr/bin/env python3
"""
Eran OS - Comprehensive Automated Kernel Verification Suite
Validates:
1. Bootloader MBR binary geometry & signature (512 bytes, 0xAA55)
2. Protected mode transition & GDT loading (User Selectors & TSS 0x28)
3. Freestanding C kernel execution at 0x10000
4. Interrupt Descriptor Table (IDT) & 8259 PIC remapping
5. Hardware timer (PIT 100Hz) & PS/2 keyboard driver initialization
6. Physical Memory Manager (PMM) frame bitmap allocator (32 MB)
7. Dynamic Kernel Heap allocator (kmalloc/kfree, 4 MB)
8. Virtual Memory Management & Two-Level Paging (CR0.PG=1)
9. System Call Interface (INT 0x80 handler)
10. Process management & multitasking scheduler
11. PCI bus peripheral device discovery
12. Parallel ATA / IDE storage controller initialization
13. Virtual Filesystem (VFS) & in-memory RamFS mount
14. USB Host Controller & device enumeration
15. Intel 82540EM (e1000) Gigabit NIC & IPv4/ICMP network stack
16. Audio subsystem & PC Speaker sound synthesis
17. ACPI power management & laptop battery telemetry
18. CPUID architecture & 64-bit Long Mode capability probe
19. Native unprivileged Offline AI Assistant Core
20. Interactive shell commands & response verification
"""

import os
import sys
import subprocess
import time

BUILD_DIR = "build"
BOOT_BIN = os.path.join(BUILD_DIR, "boot.bin")
KERNEL_BIN = os.path.join(BUILD_DIR, "kernel.bin")
IMG_FILE = os.path.join(BUILD_DIR, "eran_os.img")

EXPECTED_BOOT_STRINGS = [
    "Eran OS Bootloader v0.3.0",
    "[BOOT] 180 sectors (90 KB) loaded to 0x10000",
    "ERAN OS - KERNEL v0.3.0",
    "Target: x86 32-bit Protected Mode",
    "GDT active (Kernel/User Ring 0/3 segments + TSS 0x28).",
    "IDT active (32 CPU Exceptions + 16 IRQs remapped).",
    "Initializing 8254 PIT Timer (100 Hz)...",
    "Initializing PS/2 Keyboard Driver (IRQ 1)...",
    "Hardware interrupts globally enabled.",
    "PMM initialized.",
    "Kernel Heap initialized",
    "VMM Paging active (CR0.PG=1, lower 16MB identity mapped).",
    "System call dispatcher active (SYS_YIELD, SYS_EXIT, SYS_WRITE...).",
    "Multitasking scheduler active (PID 0: kernel_main, PID 1: bg_monitor).",
    "Scanning PCI Bus for hardware controllers...",
    "Primary ATA controller initialized",
    "VFS root '/' mounted with initial system files.",
    "USB host controller initialized.",
    "Network initialized (eth0: 192.168.1.50/24).",
    "Audio tone generator online.",
    "ACPI tables parsed & power management ready.",
    "CPUID probed",
    "Ring 3 unprivileged AI Engine active.",
    "[STATUS] Eran OS kernel initialization completed successfully!",
    "eran>"
]

EXPECTED_SHELL_COMMANDS = [
    ("version", "Eran OS Kernel v0.3.0"),
    ("vmm", "VIRTUAL MEMORY & TWO-LEVEL PAGING (VMM)"),
    ("ifconfig", "NETWORK INTERFACE CONFIGURATION (eth0)"),
    ("ping 192.168.1.1", "4 packets transmitted, 4 received, 0% packet loss"),
    ("cpufeatures", "CPU ARCHITECTURE & EXTENDED INSTRUCTION SET"),
    ("acpi", "ADVANCED CONFIGURATION & POWER INTERFACE (ACPI)"),
    ("battery", "LAPTOP POWER & BATTERY TELEMETRY"),
    ("ai who are you", "I am the Eran OS Native AI Assistant!"),
    ("ai what is the uptime", "queried via INT 0x80 SYS_UPTIME"),
    ("ai show memory", "Total RAM: 32 MB"),
    ("ai list files", "README.txt"),
    ("syscall", "INT 0x80 successfully executed")
]

def log(msg, status="INFO"):
    print(f"[{status}] {msg}")

def test_geometry():
    log("Verifying bootloader and kernel binaries...", "TEST")
    if not os.path.exists(BOOT_BIN):
        log(f"Missing {BOOT_BIN}", "FAIL")
        return False
    if os.path.getsize(BOOT_BIN) != 512:
        log("Bootloader is not 512 bytes", "FAIL")
        return False

    with open(BOOT_BIN, "rb") as f:
        data = f.read()
    if data[510] != 0x55 or data[511] != 0xAA:
        log("Invalid boot signature", "FAIL")
        return False

    if not os.path.exists(KERNEL_BIN):
        log(f"Missing {KERNEL_BIN}", "FAIL")
        return False

    kernel_size = os.path.getsize(KERNEL_BIN)
    log(f"Boot sector = 512 bytes (MBR verified). Kernel binary = {kernel_size} bytes.", "PASS")

    if not os.path.exists(IMG_FILE):
        log(f"Missing disk image {IMG_FILE}", "FAIL")
        return False

    img_size = os.path.getsize(IMG_FILE)
    if img_size != 1440 * 1024:
        log(f"Invalid disk image size: {img_size}", "FAIL")
        return False

    log(f"Disk image = {img_size} bytes (1.44MB standard geometry verified).", "PASS")
    return True

def test_kernel_execution():
    log("Running Eran OS kernel in QEMU and executing shell commands...", "TEST")

    cmd = [
        "qemu-system-i386",
        "-drive", f"format=raw,if=floppy,file={IMG_FILE}",
        "-net", "nic,model=e1000",
        "-net", "user",
        "-display", "none",
        "-serial", "stdio"
    ]

    try:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except Exception as e:
        log(f"Failed to launch QEMU: {e}", "FAIL")
        return False

    time.sleep(1.0) # Let kernel initialize

    # Send commands with pacing
    for cmd_str, _ in EXPECTED_SHELL_COMMANDS:
        proc.stdin.write(f"{cmd_str}\n")
        proc.stdin.flush()
        time.sleep(0.2)

    time.sleep(0.5)
    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()

    stdout, _ = proc.communicate()

    # 1. Check Boot Subsystems
    log("Verifying boot sequence and subsystem initializations...", "TEST")
    boot_ok = True
    for expected in EXPECTED_BOOT_STRINGS:
        if expected in stdout:
            log(f"Subsystem verified: '{expected}'", "PASS")
        else:
            log(f"Missing boot output: '{expected}'", "FAIL")
            boot_ok = False

    # 2. Check Shell Commands
    log("Verifying shell commands and responses...", "TEST")
    cmd_ok = True
    for cmd_str, expected in EXPECTED_SHELL_COMMANDS:
        if expected in stdout:
            log(f"Command '{cmd_str}' -> Output matched: '{expected}'", "PASS")
        else:
            log(f"Command '{cmd_str}' -> Missing expected output: '{expected}'", "FAIL")
            cmd_ok = False

    if not boot_ok or not cmd_ok:
        log("Full QEMU serial log dump:", "DIAG")
        print(stdout)
        return False

    return True

def main():
    print("=" * 65)
    print("  Eran OS - Comprehensive Automated Kernel Verification Suite")
    print("=" * 65)

    success = True
    if not test_geometry():
        success = False
    if not test_kernel_execution():
        success = False

    print("=" * 65)
    if success:
        print("[RESULT] ALL OPERATING SYSTEM SUBSYSTEM TESTS PASSED.")
        print("=" * 65)
        sys.exit(0)
    else:
        print("[RESULT] SUBSYSTEM VERIFICATION FAILED.")
        print("=" * 65)
        sys.exit(1)

if __name__ == "__main__":
    main()
