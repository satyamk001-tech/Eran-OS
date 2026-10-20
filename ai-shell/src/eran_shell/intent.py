"""
Natural language intent classification for Eran OS AI Shell.
"""

from enum import Enum, auto

class IntentType(Enum):
    FILE_SEARCH     = auto()
    READ_DOCUMENT   = auto()
    DEVICE_QUERY    = auto()
    SYSTEM_STATUS   = auto()
    SETTING_CHANGE  = auto()
    HELP            = auto()
    UNKNOWN         = auto()

class IntentClassifier:
    @staticmethod
    def classify(query: str) -> IntentType:
        q = query.lower().strip()

        if q in ["help", "?", "commands", "what can you do"]:
            return IntentType.HELP

        if any(w in q for w in ["find", "search", "where is", "locate", "list files", "ls"]):
            return IntentType.FILE_SEARCH

        if any(w in q for w in ["read", "show content", "cat", "view", "open file", "summarize"]):
            return IntentType.READ_DOCUMENT

        if any(w in q for w in ["device", "pci", "hardware", "controller", "usb", "keyboard", "mouse"]):
            return IntentType.DEVICE_QUERY

        if any(w in q for w in ["memory", "ram", "cpu", "uptime", "version", "status", "os info"]):
            return IntentType.SYSTEM_STATUS

        if any(w in q for w in ["set", "volume", "brightness", "configure"]):
            return IntentType.SETTING_CHANGE

        return IntentType.UNKNOWN
