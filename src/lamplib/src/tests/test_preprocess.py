import os
import tempfile
import shutil
import unittest
from unittest.mock import patch

from genny.tasks import preprocess

class TestPreprocess(unittest.TestCase):
    def setUp(self):
        self.workspace_root = tempfile.mkdtemp()

    def cleanUp(self):
        shutil.rmtree(self.workspace_root)

    def test_scoped_contest(self):
        context = preprocess.Context()
        with context.enter():
            outer = {}
            outer["outerKey"] = "outerVal"
            context.insert("outerName", outer, preprocess.ContextType.Parameter)

            with context.enter():
                inner = {}
                inner["innerKey1"] = "innerVal1"
                context.insert("innerName1", inner, preprocess.ContextType.Parameter)

                retrievedOuter = context.get("outerName", preprocess.ContextType.Parameter)
                self.assertEqual(retrievedOuter, outer)

                retrievedInner = context.get("innerName1", preprocess.ContextType.Parameter)
                self.assertEqual(retrievedInner, inner)

            with context.enter():
                inner = {}
                inner["innerKey2"] = "innerVal2"
                context.insert("innerName2", inner, preprocess.ContextType.Parameter)

                retrievedOuter = context.get("outerName", preprocess.ContextType.Parameter)
                self.assertEqual(retrievedOuter, outer)

                retrievedInner = context.get("innerName2", preprocess.ContextType.Parameter)
                self.assertEqual(retrievedInner, inner)

                retrievedOldInner = context.get("innerName1", preprocess.ContextType.Parameter)
                self.assertNotEqual(retrievedOldInner, inner)

            retrievedOuter = context.get("outerName", preprocess.ContextType.Parameter)
            self.assertEqual(retrievedOuter, outer)

            with self.assertRaises(preprocess.ParseException):
                context.get("outerName", preprocess.ContextType.ActorTemplate)

