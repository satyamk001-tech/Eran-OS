"""
Eran OS - Offline AI Productivity Shell CLI Entry Point.
"""

import sys
import os
import argparse

# Allow running directly as script without installed package
if __package__ is None or __package__ == "":
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from eran_shell.interface import print_banner, print_status, print_plan, prompt_user
    from eran_shell.providers.rule_based import RuleBasedProvider
    from eran_shell.policy import SecurityPolicyEngine
    from eran_shell.executor import TaskExecutor
else:
    from .interface import print_banner, print_status, print_plan, prompt_user
    from .providers.rule_based import RuleBasedProvider
    from .policy import SecurityPolicyEngine
    from .executor import TaskExecutor

def process_query(query: str, provider, policy, executor):
    if not query:
        return

    # 1. Generate structured action plan
    plan = provider.generate_plan(query, {})
    if not plan.actions:
        print_status(f"No executable plan found for query: '{query}'", "WARN")
        return

    print_plan(plan.explanation, plan.actions)

    # 2. Validate actions against security policy
    for act in plan.actions:
        approved, msg = policy.validate_action(act)
        if not approved:
            print_status(f"Security Policy Rejected action [{act.action_type}]: {msg}", "ERROR")
            return

        if act.requires_confirmation:
            confirm = input(f"Action '{act.action_type}' is destructive. Proceed? (y/N): ").strip().lower()
            if confirm != "y":
                print_status("Action cancelled by user.", "WARN")
                return

        # 3. Execute approved action safely
        res = executor.execute(act)
        if res.get("success"):
            print_status(f"Action [{act.action_type}] completed successfully.", "SUCCESS")
            for k, v in res.items():
                if k not in ["success", "simulated"]:
                    if isinstance(v, list):
                        print(f"  {k} ({len(v)} items):")
                        for item in v[:10]:
                            print(f"    - {item}")
                        if len(v) > 10:
                            print(f"    ... and {len(v) - 10} more.")
                    elif isinstance(v, dict):
                        print(f"  {k}:")
                        for sub_k, sub_v in v.items():
                            print(f"    {sub_k}: {sub_v}")
                    else:
                        print(f"  {k}: {v}")
        else:
            print_status(f"Action [{act.action_type}] failed: {res.get('error')}", "ERROR")

def main():
    parser = argparse.ArgumentParser(description="Eran OS Offline AI Assistant Shell")
    parser.add_argument("-c", "--command", type=str, help="Execute single query and exit")
    parser.add_argument("--dry-run", action="store_true", help="Simulate execution without modifying files")
    parser.add_argument("--workspace", type=str, default=".", help="Allowed workspace directory path")
    args = parser.parse_args()

    workspace_root = os.path.realpath(args.workspace)
    policy = SecurityPolicyEngine(workspace_root)
    executor = TaskExecutor(workspace_root, dry_run=args.dry_run)
    provider = RuleBasedProvider()

    if args.command:
        process_query(args.command, provider, policy, executor)
        return

    print_banner()
    while True:
        query = prompt_user("eran-ai> ")
        if query.lower() in ["exit", "quit", "q"]:
            print_status("Session terminated. Goodbye.", "INFO")
            break
        process_query(query, provider, policy, executor)

if __name__ == "__main__":
    main()
