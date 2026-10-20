"""
Abstract base class for Eran OS AI model providers.
"""

from abc import ABC, abstractmethod
from typing import Dict, Any
from ..planner import Plan

class ModelProvider(ABC):
    @abstractmethod
    def generate_plan(self, user_query: str, context: Dict[str, Any]) -> Plan:
        """
        Interprets natural language user query and constructs a structured Plan.
        """
        pass
