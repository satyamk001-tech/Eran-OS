"""
Structured action planning and schemas for Eran OS AI Shell.
The AI engine produces strongly-typed action records, never raw shell strings.
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List

@dataclass
class Action:
    action_type: str
    params: Dict[str, Any]
    is_destructive: bool = False
    requires_confirmation: bool = False

@dataclass
class Plan:
    explanation: str
    actions: List[Action] = field(default_factory=list)

class ActionTypes:
    FIND_FILES      = "FindFiles"
    READ_FILE       = "ReadFile"
    QUERY_DEVICES   = "QueryDevices"
    GET_SYS_INFO    = "GetSystemInfo"
    CHANGE_SETTING  = "ChangeSetting"
    DELETE_FILE     = "DeleteFile"
    EXEC_DIAGNOSTIC = "ExecuteDiagnostic"
