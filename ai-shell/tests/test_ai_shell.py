"""
Automated unit and security tests for Eran OS AI Shell.
"""

import unittest
import os
import sys

# Ensure src is on path
src_dir = os.path.realpath(os.path.join(os.path.dirname(__file__), "../src"))
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)

from eran_shell.intent import IntentClassifier, IntentType
from eran_shell.providers.rule_based import RuleBasedProvider
from eran_shell.planner import ActionTypes
from eran_shell.policy import SecurityPolicyEngine
from eran_shell.executor import TaskExecutor

class TestIntentClassifier(unittest.TestCase):
    def test_classification(self):
        self.assertEqual(IntentClassifier.classify("find files named main.c"), IntentType.FILE_SEARCH)
        self.assertEqual(IntentClassifier.classify("read file README.md"), IntentType.READ_DOCUMENT)
        self.assertEqual(IntentClassifier.classify("show all hardware devices"), IntentType.DEVICE_QUERY)
        self.assertEqual(IntentClassifier.classify("how much memory is free?"), IntentType.SYSTEM_STATUS)
        self.assertEqual(IntentClassifier.classify("set volume to 80%"), IntentType.SETTING_CHANGE)
        self.assertEqual(IntentClassifier.classify("help"), IntentType.HELP)

class TestSecurityPolicy(unittest.TestCase):
    def setUp(self):
        self.workspace = os.path.realpath(os.path.join(os.path.dirname(__file__), "../.."))
        self.policy = SecurityPolicyEngine(self.workspace)

    def test_valid_path(self):
        valid, resolved = self.policy.validate_path("README.md")
        self.assertTrue(valid)
        self.assertTrue(resolved.startswith(self.workspace))

    def test_path_traversal_blocked(self):
        valid, msg = self.policy.validate_path("../../etc/shadow")
        self.assertFalse(valid)
        self.assertIn("Path traversal violation", msg)

    def test_absolute_escape_blocked(self):
        valid, msg = self.policy.validate_path("/etc/passwd")
        self.assertFalse(valid)
        self.assertIn("Path traversal violation", msg)

class TestExecutor(unittest.TestCase):
    def setUp(self):
        self.workspace = os.path.realpath(os.path.join(os.path.dirname(__file__), "../.."))
        self.executor = TaskExecutor(self.workspace)
        self.provider = RuleBasedProvider()

    def test_plan_generation(self):
        plan = self.provider.generate_plan("show connected devices", {})
        self.assertGreater(len(plan.actions), 0)
        self.assertEqual(plan.actions[0].action_type, ActionTypes.QUERY_DEVICES)

    def test_device_execution(self):
        plan = self.provider.generate_plan("show hardware devices", {})
        res = self.executor.execute(plan.actions[0])
        self.assertTrue(res.get("success"))
        self.assertIn("devices", res)
        self.assertGreater(len(res["devices"]), 0)

    def test_dry_run(self):
        dry_executor = TaskExecutor(self.workspace, dry_run=True)
        plan = self.provider.generate_plan("read file README.md", {})
        res = dry_executor.execute(plan.actions[0])
        self.assertTrue(res.get("success"))
        self.assertTrue(res.get("simulated"))

if __name__ == "__main__":
    unittest.main()
