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

    def _runParse(self, yaml_input):
        cwd = os.getcwd()

        p = preprocess._WorkloadParser()
        parsedConfig = p.parse(
            yaml_input=yaml_input, source=preprocess._WorkloadParser.YamlSource.String, path=cwd
        )

        return parsedConfig

    def _assertYaml(self, yaml_input, expected):
        self.assertEqual(self._runParse(yaml_input), expected)

    def _assertParseException(self, yaml_input):
        self.assertRaises(preprocess.ParseException, self._runParse, yaml_input)


    def test_scoped_contest(self):
        context = preprocess._Context()
        with context.enter():
            outer = {}
            outer["outerKey"] = "outerVal"
            context.insert("outerName", outer, preprocess._ContextType.Parameter)

            with context.enter():
                inner = {}
                inner["innerKey1"] = "innerVal1"
                context.insert("innerName1", inner, preprocess._ContextType.Parameter)

                retrievedOuter = context.get("outerName", preprocess._ContextType.Parameter)
                self.assertEqual(retrievedOuter, outer)

                retrievedInner = context.get("innerName1", preprocess._ContextType.Parameter)
                self.assertEqual(retrievedInner, inner)

            with context.enter():
                inner = {}
                inner["innerKey2"] = "innerVal2"
                context.insert("innerName2", inner, preprocess._ContextType.Parameter)

                retrievedOuter = context.get("outerName", preprocess._ContextType.Parameter)
                self.assertEqual(retrievedOuter, outer)

                retrievedInner = context.get("innerName2", preprocess._ContextType.Parameter)
                self.assertEqual(retrievedInner, inner)

                retrievedOldInner = context.get("innerName1", preprocess._ContextType.Parameter)
                self.assertNotEqual(retrievedOldInner, inner)

            retrievedOuter = context.get("outerName", preprocess._ContextType.Parameter)
            self.assertEqual(retrievedOuter, outer)

            with self.assertRaises(preprocess.ParseException):
                context.get("outerName", preprocess._ContextType.ActorTemplate)

    def test_scoped_parameters(self):

        yaml_input = """
ActorTemplates:
- TemplateName: TestTemplate1
  Config:
    Name: {^Parameter: {Name: "Name", Default: "DefaultValue"}}
    SomeKey: SomeValue
    Phases:
      OnlyActiveInPhases:
        Active: [{^Parameter: {Name: "Phase", Default: 1}}]
        NopInPhasesUpTo: 3
        PhaseConfig:
          Duration: {^Parameter: {Name: "Duration", Default: 3 minutes}}

- TemplateName: TestTemplate2
  Config:
    Name: {^Parameter: {Name: "Name", Default: "DefaultValue"}}
    SomeKey: SomeValue
    Phases:
      - Nop: true
      - Nop: true
      - ExternalPhaseConfig:
          Path: src/testlib/phases/Good.yml
          Parameters:
            Repeat: 2
      - Nop: true
    AnotherValueFromRepeat: {^Parameter: {Name: "Repeat", Default: "BadDefault"}}

Actors:
- ActorFromTemplate:
    TemplateName: TestTemplate1
    TemplateParameters:
      Name: ActorName1
      Phase: 0
      Duration: 5 minutes

# Lacking the specified duration, we expect the default duration to be used,
# instead of the one from the previous ActorFromTemplate which was scoped to that block.
- ActorFromTemplate:
    TemplateName: TestTemplate1
    TemplateParameters:
      Phase: 1
      Name: ActorName2

# The value of Repeat should be correctly "shadowed" in the lower level external phase.
- ActorFromTemplate:
    TemplateName: TestTemplate2
    TemplateParameters:
      Name: ActorName3
      Repeat: GoodValue
"""

        expected = """Actors:
- Name: ActorName1
  SomeKey: SomeValue
  Phases:
  - Duration: 5 minutes
  - &id001
    Nop: true
  - *id001
  - *id001
- Name: ActorName2
  SomeKey: SomeValue
  Phases:
  - &id002
    Nop: true
  - Duration: 3 minutes
  - *id002
  - *id002
- Name: ActorName3
  SomeKey: SomeValue
  Phases:
  - Nop: true
  - Nop: true
  - Repeat: 2
    Mode: NoException
  - Nop: true
  AnotherValueFromRepeat: GoodValue
"""

        self._assertYaml(yaml_input, expected)

    def test_preprocess_keywords(self):
        yaml_input = """
ActorTemplates:
- TemplateName: TestTemplate
  Config:
    Name: {^Parameter: {Name: "Name", Default: "IncorrectDefault"}}
    SomeKey: SomeValue
    Phases:
      OnlyActiveInPhases:
        Active: [{^Parameter: {Name: "Phase", Default: 1}}]
        NopInPhasesUpTo: 3
        PhaseConfig:
          Duration: {^Parameter: {Name: "Duration", Default: 3 minutes}}
Actors:
- ActorFromTemplate:
    TemplateName: TestTemplate
    TemplateParameters:
      Name: ActorName1
      Phase: 0
      Duration: 5 minutes
- ActorFromTemplate:
    TemplateName: TestTemplate
    TemplateParameters:
      Phase: 1
      Name: ActorName2"""

        expected = """Actors:
- Name: ActorName1
  SomeKey: SomeValue
  Phases:
  - Duration: 5 minutes
  - &id001
    Nop: true
  - *id001
  - *id001
- Name: ActorName2
  SomeKey: SomeValue
  Phases:
  - &id002
    Nop: true
  - Duration: 3 minutes
  - *id002
  - *id002
"""

        self._assertYaml(yaml_input, expected)

    def test_load_external_default_param(self):
        
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - ExternalPhaseConfig:
        Path: src/testlib/phases/Good.yml'''

        expected = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 1
    Mode: NoException
'''

        self._assertYaml(yaml_input, expected)


    def test_load_external_default_param_inner_key(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - ExternalPhaseConfig:
      Path: src/testlib/phases/GoodWithKey.yml
      Key: ForSelfTest'''

        expected = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 1
    Mode: NoException
'''

        self._assertYaml(yaml_input, expected)

    def test_external_override_param(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - ExternalPhaseConfig:
      Path: src/testlib/phases/Good.yml
      Parameters:
        Repeat: 2
'''
        expected = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 2
    Mode: NoException
'''
        self._assertYaml(yaml_input, expected)

    #SECTION("With Inline Parameter") {
    def test_external_inline_param(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - ExternalPhaseConfig:
      Path: "src/testlib/phases/GoodNoRepeat.yml"
    Repeat: 3
'''
        expected = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Mode: NoException
    Repeat: 3
'''
        self._assertYaml(yaml_input, expected)


    #SECTION("Load Bad External State 1") {
    def test_bad_external_state_1(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - ExternalPhaseConfig:
      Path: src/testlib/phases/MissingAllFields.yml
      Parameters:
        Repeat: 2
'''

        self._assertParseException(yaml_input)


    def test_bad_external_state_2(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - ExternalPhaseConfig:
        Path: src/testlib/phases/MissingDefault.yml
        Parameters:
          Repeat: 2
'''
        self._assertParseException(yaml_input)


    def test_bad_external_state_3(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - ExternalPhaseConfig:
        Path: src/testlib/phases/MissingName.yml
        Parameters:
          Repeat: 2
'''
        self._assertParseException(yaml_input)


    def test_bad_external_state_4(self):
        yaml_input = '''SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - ExternalPhaseConfig:
        Path: "src/testlib/phases/MissingSchemaVersion.yml"
'''
        self._assertParseException(yaml_input)
