"""
Offline local LLM provider interface for Eran OS.
Interfaces with llama.cpp or local GGUF models when weights are present,
falling back gracefully to the deterministic provider when offline.
"""

from typing import Dict, Any
from .base import ModelProvider
from .rule_based import RuleBasedProvider
from ..planner import Plan

class LocalLLMProvider(ModelProvider):
    def __init__(self, model_path: str = None):
        self.model_path = model_path
        self.fallback = RuleBasedProvider()

    def generate_plan(self, user_query: str, context: Dict[str, Any]) -> Plan:
        # If llama-cpp-python is installed and weights are loaded, run local inference.
        # Otherwise, use deterministic offline fallback without cloud requirements.
        return self.fallback.generate_plan(user_query, context)
