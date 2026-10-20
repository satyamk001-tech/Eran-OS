#!/usr/bin/env python3
import subprocess
import time
import sys

IMG_FILE = "build/eran_os.img"

def run_test():
    print("=================================================================")
    print("  Eran OS - Mini Linux Environment & HTML Browser Test Suite     ")
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

    test_commands = [
        ("pwd", "/"),
        ("cd /bin", "root@eranos:/bin eran>"),
        ("pwd", "/bin"),
        ("uname -a", "Linux eranos 0.4.0 #1 SMP i386 GNU/Linux"),
        ("whoami", "root"),
        ("which ls", "/bin/ls"),
        ("which browser", "/bin/browser"),
        ("env", "OS=EranOS-MiniLinux"),
        ("cat /etc/os-release", "PRETTY_NAME=\"Eran OS Mini Linux 0.4.0 (x86_32)\""),
        ("browser http://eranos.local/", "Welcome to Eran OS Web Portal"),
        ("browser http://eranos.local/about.html", "About Eran OS Workstation"),
        ("browser http://eranos.local/help.html", "Mini Linux Command Reference"),
        ("browser search", "EranSearch: Offline Knowledge Index"),
        ("browser google.com", "External Web Notice: DNS & HTTPS"),
        ("browser help", "Mini Linux Command Reference"),
        ("browser about", "About Eran OS Workstation")
    ]

    for c, _ in test_commands:
        proc.stdin.write(f"{c}\n")
        proc.stdin.flush()
        time.sleep(0.3)

    time.sleep(1.0)
    proc.terminate()
    try:
        proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()

    stdout, _ = proc.communicate()

    all_passed = True
    for cmd_str, expected in test_commands:
        if expected in stdout:
            print(f"[PASS] Command '{cmd_str}' -> Found: '{expected}'")
        else:
            print(f"[FAIL] Command '{cmd_str}' -> Missing: '{expected}'")
            all_passed = False

    print("=================================================================")
    if all_passed:
        print("[RESULT] ALL MINI LINUX & WEB BROWSER TESTS PASSED.")
    else:
        print("[RESULT] SOME MINI LINUX TESTS FAILED.")
    print("=================================================================")
    return all_passed

if __name__ == "__main__":
    success = run_test()
    sys.exit(0 if success else 1)
