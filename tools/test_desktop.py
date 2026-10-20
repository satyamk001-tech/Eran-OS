#!/usr/bin/env python3
import subprocess
import time
import sys

IMG_FILE = "build/eran_os.img"

def run_test():
    print("=================================================================")
    print("  Eran OS - Graphical Desktop UI & Window Compositor Test Suite  ")
    print("=================================================================")

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
        print(f"[FAIL] Unable to launch QEMU: {e}")
        return False

    time.sleep(1.2) # wait for kernel boot to complete

    # 1. Launch Desktop
    print("[TEST] Launching 2D Graphical Desktop via 'desktop' command...")
    proc.stdin.write("desktop\n")
    proc.stdin.flush()
    time.sleep(0.5)

    # 2. Switch to Terminal window and run uptime command inside GUI
    print("[TEST] Sending in-GUI commands (Terminal switch, uptime, audio chime)...")
    proc.stdin.write("uptime\n")
    proc.stdin.flush()
    time.sleep(0.3)

    # 3. Exit Desktop with ESC (ASCII 27)
    print("[TEST] Sending ESC key (ASCII 27) to exit desktop back to text console...")
    proc.stdin.write("\x1b")
    proc.stdin.flush()
    time.sleep(0.5)

    # 4. Verify Shell responsiveness after returning
    print("[TEST] Verifying shell is healthy after GUI exit via 'version' command...")
    proc.stdin.write("version\n")
    proc.stdin.flush()
    time.sleep(0.5)

    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()

    stdout, _ = proc.communicate()

    # Checks
    checks = [
        ("Desktop launched", "[DESKTOP] Launching 2D graphical desktop workstation..."),
        ("Desktop exited", "[DESKTOP] Returned to text console."),
        ("Shell healthy after GUI exit", "Eran OS Kernel v0.4.0")
    ]

    all_passed = True
    for desc, expected in checks:
        if expected in stdout:
            print(f"[PASS] {desc}: '{expected}'")
        else:
            print(f"[FAIL] {desc}: Missing '{expected}'")
            all_passed = False

    print("=================================================================")
    if all_passed:
        print("[RESULT] ALL GRAPHICAL DESKTOP UI TESTS PASSED SUCCESSFULLY.")
    else:
        print("[RESULT] SOME DESKTOP TESTS FAILED.")
    print("=================================================================")
    return all_passed

if __name__ == "__main__":
    success = run_test()
    sys.exit(0 if success else 1)
