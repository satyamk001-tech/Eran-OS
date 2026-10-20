"""
Terminal user interface and formatting for the Eran OS AI Shell.
"""

import sys

class Colors:
    CYAN    = "\033[96m"
    GREEN   = "\033[92m"
    YELLOW  = "\033[93m"
    RED     = "\033[91m"
    BOLD    = "\033[1m"
    DIM     = "\033[2m"
    RESET   = "\033[0m"

def print_banner():
    print(f"{Colors.CYAN}{Colors.BOLD}==================================================================={Colors.RESET}")
    print(f"{Colors.CYAN}{Colors.BOLD}             ERAN OS — OFFLINE AI PRODUCTIVITY SHELL               {Colors.RESET}")
    print(f"{Colors.CYAN}{Colors.BOLD}==================================================================={Colors.RESET}")
    print(f"{Colors.DIM}Running in secure unprivileged mode (Host Ring 3 / Sandboxed).{Colors.RESET}")
    print(f"{Colors.DIM}Type natural language requests, OS commands, or 'exit' to quit.{Colors.RESET}\n")

def print_status(msg: str, status: str = "INFO"):
    color = Colors.CYAN
    if status == "SUCCESS":
        color = Colors.GREEN
    elif status == "WARN":
        color = Colors.YELLOW
    elif status == "ERROR":
        color = Colors.RED
    print(f"{color}[{status}]{Colors.RESET} {msg}")

def print_plan(explanation: str, actions: list):
    print(f"\n{Colors.BOLD}AI Plan:{Colors.RESET} {explanation}")
    for idx, act in enumerate(actions, 1):
        destr = f" {Colors.RED}[DESTRUCTIVE]{Colors.RESET}" if act.is_destructive else ""
        print(f"  {idx}. {Colors.CYAN}{act.action_type}{Colors.RESET}{destr} -> {act.params}")

def prompt_user(query: str) -> str:
    try:
        return input(f"{Colors.GREEN}{Colors.BOLD}eran-ai>{Colors.RESET} ").strip()
    except (EOFError, KeyboardInterrupt):
        print("\nExiting.")
        sys.exit(0)
