#!/usr/bin/env python3
"""
Eran OS - Bootloader Verification Test Suite
Automated regression test running in headless QEMU.
Validates boot sector geometry, MBR signature, and COM1 serial transmission.
"""

import os
import sys
import subprocess
import time

BUILD_DIR = "build"
BIN_FILE = os.path.join(BUILD_DIR, "main.bin")
IMG_FILE = os.path.join(BUILD_DIR, "main_floppy.img")

EXPECTED_STRINGS = [
    "Eran OS - Stage 1 Bootloader v0.1.0",
    "[BOOT] CPU real mode initialized (CS:0000 SP:7C00).",
    "[BOOT] BIOS INT 10h display and COM1 serial active.",
    "[BOOT] Drive ID preserved for Stage 2 disk load.",
    "[BOOT] System halted. Ready for Phase 2.",
]

def log(msg, status="INFO"):
    print(f"[{status}] {msg}")

def test_binary_geometry():
    log("Verifying raw bootloader binary geometry...", "TEST")
    if not os.path.exists(BIN_FILE):
        log(f"Binary file not found: {BIN_FILE}", "FAIL")
        return False

    size = os.path.getsize(BIN_FILE)
    if size != 512:
        log(f"Invalid boot sector size: {size} bytes (expected 512 bytes)", "FAIL")
        return False

    with open(BIN_FILE, "rb") as f:
        data = f.read()

    # Verify MBR boot signature at byte offset 510-511 (0x55, 0xAA)
    sig_low = data[510]
    sig_high = data[511]
    if sig_low != 0x55 or sig_high != 0xAA:
        log(f"Invalid boot signature: 0x{sig_high:02X}{sig_low:02X} (expected 0xAA55)", "FAIL")
        return False

    log(f"Binary size = 512 bytes, MBR signature = 0xAA55 verified.", "PASS")
    return True

def test_floppy_geometry():
    log("Verifying floppy disk image geometry...", "TEST")
    if not os.path.exists(IMG_FILE):
        log(f"Floppy image not found: {IMG_FILE}", "FAIL")
        return False

    size = os.path.getsize(IMG_FILE)
    expected_size = 1440 * 1024  # 1,474,560 bytes
    if size != expected_size:
        log(f"Invalid floppy image size: {size} bytes (expected {expected_size} bytes)", "FAIL")
        return False

    log(f"Floppy image size = {size} bytes (1.44 MB standard 3.5\" geometry) verified.", "PASS")
    return True

def test_qemu_boot():
    log("Running Eran OS bootloader in QEMU (headless, serial=stdio)...", "TEST")
    cmd = [
        "qemu-system-i386",
        "-drive", f"format=raw,if=floppy,file={IMG_FILE}",
        "-display", "none",
        "-serial", "stdio"
    ]

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        # Allow QEMU sufficient time to run BIOS post and boot sector
        time.sleep(1.5)
        proc.terminate()
        try:
            stdout, stderr = proc.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            stdout, stderr = proc.communicate()

    except FileNotFoundError:
        log("qemu-system-i386 executable not found in PATH", "FAIL")
        return False
    except Exception as e:
        log(f"Failed to execute QEMU: {e}", "FAIL")
        return False

    # Check for expected serial banner lines
    all_matched = True
    for expected in EXPECTED_STRINGS:
        if expected in stdout:
            log(f"Matched serial output: '{expected}'", "PASS")
        else:
            log(f"Missing expected output: '{expected}'", "FAIL")
            all_matched = False

    if not all_matched:
        log("QEMU stdout was:", "DIAG")
        print(stdout)
        if stderr:
            log("QEMU stderr was:", "DIAG")
            print(stderr)
        return False

    return True

def main():
    print("=" * 60)
    print("  Eran OS - Bootloader Automated Verification Suite")
    print("=" * 60)

    success = True
    if not test_binary_geometry():
        success = False
    if not test_floppy_geometry():
        success = False
    if not test_qemu_boot():
        success = False

    print("=" * 60)
    if success:
        print("[RESULT] ALL BOOT TESTS PASSED.")
        print("=" * 60)
        sys.exit(0)
    else:
        print("[RESULT] BOOT TESTS FAILED.")
        print("=" * 60)
        sys.exit(1)

if __name__ == "__main__":
    main()
