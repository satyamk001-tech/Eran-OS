"""
Security policy validator and sandbox enforcement engine for Eran OS AI Shell.
Guarantees that model actions conform to permission boundaries before execution.
"""

import os
from typing import Tuple
from .planner import Action, ActionTypes

class SecurityPolicyEngine:
    def __init__(self, workspace_root: str):
        self.workspace_root = os.path.realpath(workspace_root)

    def validate_path(self, target_path: str) -> Tuple[bool, str]:
        """
        Validates that target_path resolves strictly within workspace_root.
        Prevents directory traversal attacks ('../') and symlink escapes.
        """
        if not target_path:
            return False, "Empty path provided."

        # If path is relative, join with workspace root
        if not os.path.isabs(target_path):
            resolved = os.path.realpath(os.path.join(self.workspace_root, target_path))
        else:
            resolved = os.path.realpath(target_path)

        # Enforce sandbox boundary
        common = os.path.commonpath([self.workspace_root, resolved])
        if common != self.workspace_root:
            return False, f"Path traversal violation: '{target_path}' escapes workspace boundary."

        return True, resolved

    def validate_action(self, action: Action) -> Tuple[bool, str]:
        """
        Validates an action against capability policies.
        """
        # Validate path if present in parameters
        if "path" in action.params:
            valid, msg_or_path = self.validate_path(action.params["path"])
            if not valid:
                return False, msg_or_path
            action.params["path"] = msg_or_path

        # Sensitive actions check
        if action.action_type == ActionTypes.DELETE_FILE:
            action.is_destructive = True
            action.requires_confirmation = True

        return True, "Action approved by policy engine."
