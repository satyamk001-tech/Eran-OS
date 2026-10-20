#!/usr/bin/env python3
"""
Eran OS - AI Shell Test Runner
"""

import subprocess
import sys
import os

def main():
    print("=" * 65)
    print("  Eran OS - Offline AI Shell & Policy Test Suite")
    print("=" * 65)

    test_file = os.path.join(os.path.dirname(__file__), "../ai-shell/tests/test_ai_shell.py")
    res = subprocess.run([sys.executable, test_file])

    if res.returncode == 0:
        print("=" * 65)
        print("[RESULT] ALL AI SHELL TESTS PASSED.")
        print("=" * 65)
        sys.exit(0)
    else:
        print("=" * 65)
        print("[RESULT] AI SHELL TESTS FAILED.")
        print("=" * 65)
        sys.exit(1)

if __name__ == "__main__":
    main()
