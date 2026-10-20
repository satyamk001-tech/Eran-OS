#!/usr/bin/env python3
"""
Eran OS - Comprehensive Automated Kernel Verification Suite
Validates:
1. Bootloader MBR binary geometry & signature
2. Protected mode transition & GDT loading
3. Freestanding C kernel execution at 0x10000
4. Interrupt Descriptor Table (IDT) & 8259 PIC remapping
5. Hardware timer (PIT 100Hz) & PS/2 keyboard driver initialization
6. Physical Memory Manager (PMM) frame bitmap allocator
7. Dynamic Kernel Heap allocator (kmalloc/kfree)
8. PCI bus peripheral device discovery
9. Virtual Filesystem (VFS) & in-memory RamFS mount
10. Interactive kernel shell prompt
"""

import os
import sys
import subprocess
import time
import tempfile

BUILD_DIR = "build"
BOOT_BIN = os.path.join(BUILD_DIR, "boot.bin")
KERNEL_BIN = os.path.join(BUILD_DIR, "kernel.bin")
IMG_FILE = os.path.join(BUILD_DIR, "eran_os.img")

EXPECTED_OUTPUTS = [
    "Eran OS Bootloader v0.2.0",
    "[BOOT] Track 0 Head 0 loaded.",
    "[BOOT] Track 1 Head 0 loaded. Total 53 sectors.",
    "ERAN OS - KERNEL v0.2.0",
    "Target: x86 32-bit Protected Mode (Base: 0x10000)",
    "IDT active (32 CPU Exceptions + 16 IRQs remapped).",
    "Initializing 8254 PIT Timer (100 Hz)...",
    "Initializing PS/2 Keyboard Driver (IRQ 1)...",
    "Hardware interrupts globally enabled.",
    "PMM initialized.",
    "Kernel Heap initialized",
    "Scanning PCI Bus for hardware controllers...",
    "VFS root '/' mounted with initial system files.",
    "[STATUS] Eran OS kernel initialization completed successfully!",
    "eran>"
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
    log("Running Eran OS kernel in QEMU (headless, serial=file)...", "TEST")

    with tempfile.NamedTemporaryFile(suffix=".log", delete=False) as tf:
        serial_log_path = tf.name

    cmd = [
        "qemu-system-i386",
        "-drive", f"format=raw,if=floppy,file={IMG_FILE}",
        "-display", "none",
        "-serial", f"file:{serial_log_path}"
    ]

    try:
        proc = subprocess.Popen(cmd)
        time.sleep(2.0)
        proc.terminate()
        try:
            proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            proc.kill()
    except Exception as e:
        log(f"Failed to execute QEMU: {e}", "FAIL")
        if os.path.exists(serial_log_path):
            os.unlink(serial_log_path)
        return False

    stdout = ""
    if os.path.exists(serial_log_path):
        with open(serial_log_path, "r", errors="replace") as f:
            stdout = f.read()
        os.unlink(serial_log_path)

    all_matched = True
    for expected in EXPECTED_OUTPUTS:
        if expected in stdout:
            log(f"Matched subsystem: '{expected}'", "PASS")
        else:
            log(f"Missing expected output: '{expected}'", "FAIL")
            all_matched = False

    if not all_matched:
        log("Full QEMU serial log dump was:", "DIAG")
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
