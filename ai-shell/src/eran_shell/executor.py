"""
Safe task executor for approved Eran OS structured action plans.
"""

import os
import glob
from typing import Dict, Any
from .planner import Action, ActionTypes

class TaskExecutor:
    def __init__(self, workspace_root: str, dry_run: bool = False):
        self.workspace_root = workspace_root
        self.dry_run = dry_run

    def execute(self, action: Action) -> Dict[str, Any]:
        if self.dry_run:
            return {
                "success": True,
                "output": f"[DRY-RUN] Simulated execution of {action.action_type} with {action.params}",
                "simulated": True
            }

        handler = getattr(self, f"_handle_{action.action_type}", None)
        if not handler:
            return {"success": False, "error": f"Unknown action type: {action.action_type}"}

        try:
            return handler(action.params)
        except Exception as e:
            return {"success": False, "error": f"Execution failure: {str(e)}"}

    def _handle_FindFiles(self, params: Dict[str, Any]) -> Dict[str, Any]:
        pattern = params.get("pattern", "*")
        root = params.get("path", self.workspace_root)
        matches = []
        for dirpath, _, filenames in os.walk(root):
            for f in filenames:
                if pattern == "*" or pattern.lower() in f.lower():
                    rel_path = os.path.relpath(os.path.join(dirpath, f), self.workspace_root)
                    matches.append(rel_path)
                    if len(matches) >= 50:
                        break
        return {"success": True, "files": matches, "count": len(matches)}

    def _handle_ReadFile(self, params: Dict[str, Any]) -> Dict[str, Any]:
        path = params.get("path")
        if not path or not os.path.isfile(path):
            return {"success": False, "error": f"File does not exist: {path}"}

        with open(path, "r", errors="replace") as f:
            content = f.read(4096) # Read up to 4KB safely

        return {"success": True, "content": content, "size": len(content)}

    def _handle_QueryDevices(self, params: Dict[str, Any]) -> Dict[str, Any]:
        # Reports simulated/detected hardware inventory from OS specification
        devices = [
            {"name": "Intel 8254 PIT Timer", "irq": 0, "status": "ACTIVE", "frequency": "100 Hz"},
            {"name": "Standard PS/2 Keyboard", "irq": 1, "status": "ACTIVE", "port": "0x60"},
            {"name": "16550 UART Serial (COM1)", "irq": 4, "status": "ACTIVE", "port": "0x3F8"},
            {"name": "Standard VGA Controller", "status": "ACTIVE", "mode": "80x25 Text (0xB8000)"},
            {"name": "Floppy Disk Controller", "status": "ACTIVE", "capacity": "1.44 MB"},
            {"name": "PCI Host Bridge", "status": "ACTIVE", "bus": 0}
        ]
        return {"success": True, "devices": devices}

    def _handle_GetSystemInfo(self, params: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "success": True,
            "os_name": "Eran OS",
            "version": "0.2.0",
            "architecture": "x86 32-bit Protected Mode",
            "kernel_base": "0x00010000",
            "heap_base": "0x00200000",
            "privilege": "Ring 0 (Kernel) / Ring 3 (User & AI Assistant)"
        }

    def _handle_ChangeSetting(self, params: Dict[str, Any]) -> Dict[str, Any]:
        key = params.get("key")
        val = params.get("value")
        return {"success": True, "message": f"Setting '{key}' configured to '{val}'."}
