"""
Offline deterministic NLP provider for Eran OS AI Shell.
Guarantees 100% offline, zero-dependency, reliable intent parsing.
"""

from typing import Dict, Any
from .base import ModelProvider
from ..planner import Plan, Action, ActionTypes
from ..intent import IntentClassifier, IntentType

class RuleBasedProvider(ModelProvider):
    def generate_plan(self, user_query: str, context: Dict[str, Any]) -> Plan:
        intent = IntentClassifier.classify(user_query)
        q = user_query.strip().lower()

        if intent == IntentType.FILE_SEARCH:
            pattern = "*"
            words = user_query.split()
            for key in ["named", "called", "for", "matching"]:
                if key in [w.lower() for w in words]:
                    idx = [w.lower() for w in words].index(key)
                    if idx + 1 < len(words):
                        pattern = words[idx + 1].strip("'\",;:")
                        break
            if pattern == "*" and len(words) > 1 and words[0].lower() in ["find", "locate", "search"]:
                # If query is "find boot.asm"
                if words[1].lower() not in ["files", "file", "all"]:
                    pattern = words[1].strip("'\",;:")
                elif len(words) > 2 and words[1].lower() in ["files", "file"]:
                    pattern = words[2].strip("'\",;:")

            return Plan(
                explanation=f"Searching filesystem for pattern '{pattern}'",
                actions=[Action(ActionTypes.FIND_FILES, {"pattern": pattern})]
            )

        elif intent == IntentType.READ_DOCUMENT:
            # Extract target filename if present
            target = "README.md"
            words = user_query.split()
            for w in reversed(words):
                if "." in w or "/" in w:
                    target = w.strip("'\",;:")
                    break
            return Plan(
                explanation=f"Reading contents of file '{target}'",
                actions=[Action(ActionTypes.READ_FILE, {"path": target})]
            )

        elif intent == IntentType.DEVICE_QUERY:
            return Plan(
                explanation="Inspecting connected system hardware controllers and status",
                actions=[Action(ActionTypes.QUERY_DEVICES, {})]
            )

        elif intent == IntentType.SYSTEM_STATUS:
            return Plan(
                explanation="Retrieving kernel architecture and system memory status",
                actions=[Action(ActionTypes.GET_SYS_INFO, {})]
            )

        elif intent == IntentType.SETTING_CHANGE:
            return Plan(
                explanation="Updating system setting parameter",
                actions=[Action(ActionTypes.CHANGE_SETTING, {"key": "volume", "value": "50%"})]
            )

        elif intent == IntentType.HELP:
            return Plan(
                explanation="Displaying AI productivity assistant guidance",
                actions=[Action(ActionTypes.GET_SYS_INFO, {"help": True})]
            )

        else:
            return Plan(
                explanation=f"Could not construct a safe plan for: '{user_query}'",
                actions=[]
            )
