import os
import tempfile
import shutil
import unittest
import yaml
from io import StringIO
from unittest.mock import patch

from genny.tasks import preprocess

DEFAULT_URI = "mongodb://localhost:27017"
MONGOSTREAM_URI = "mongodb://localhost:27018"


class TestPreprocess(unittest.TestCase):
    def setUp(self):
        self.workspace_root = tempfile.mkdtemp()

    def cleanUp(self):
        shutil.rmtree(self.workspace_root)

    def _runParse(self, yaml_input):
        cwd = os.getcwd()

        p = preprocess._WorkloadParser()
        parsedConfig = p.parse(
            yaml_input=yaml_input,
            default_uri=DEFAULT_URI,
            mongostream_uri=MONGOSTREAM_URI,
            source=preprocess._WorkloadParser.YamlSource.String,
            path=cwd,
        )

        return yaml.dump(parsedConfig, sort_keys=False)

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
      - LoadConfig:
          Path: src/testlib/configs/Good.yml
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
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
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
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""

        self._assertYaml(yaml_input, expected)

    def test_load_external_default_param(self):

        yaml_input = """SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - LoadConfig:
        Path: src/testlib/configs/Good.yml"""

        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 1
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""

        self._assertYaml(yaml_input, expected)

    def test_load_external_default_param_inner_key(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - LoadConfig:
      Path: src/testlib/configs/GoodWithKey.yml
      Key: ForSelfTest"""

        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 1
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""

        self._assertYaml(yaml_input, expected)

    def test_external_override_param(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - LoadConfig:
      Path: src/testlib/configs/Good.yml
      Parameters:
        Repeat: 2
"""
        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 2
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""
        self._assertYaml(yaml_input, expected)

    # SECTION("With Inline Parameter") {
    def test_external_inline_param(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - LoadConfig:
      Path: "src/testlib/configs/GoodNoRepeat.yml"
    Repeat: 3
"""
        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Mode: NoException
    Repeat: 3
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""
        self._assertYaml(yaml_input, expected)

    # SECTION("Load Bad External State 1") {
    def test_bad_external_state_1(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - LoadConfig:
      Path: src/testlib/configs/MissingAllFields.yml
      Parameters:
        Repeat: 2
"""

        self._assertParseException(yaml_input)

    def test_bad_external_state_2(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - LoadConfig:
        Path: src/testlib/configs/MissingDefault.yml
        Parameters:
          Repeat: 2
"""
        self._assertParseException(yaml_input)

    def test_bad_external_state_3(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - LoadConfig:
        Path: src/testlib/configs/MissingName.yml
        Parameters:
          Repeat: 2
"""
        self._assertParseException(yaml_input)

    def test_bad_external_state_4(self):
        yaml_input = """SchemaVersion: 2018-07-01
Actors:
  - Type: Fails
    Name: Fails
    Threads: 1
    Phases:
    - LoadConfig:
        Path: "src/testlib/configs/MissingSchemaVersion.yml"
"""
        self._assertParseException(yaml_input)

    def test_load_entire_workload(self):
        yaml_input = """SchemaVersion: 2018-07-01

LoadConfig:
    Path: "src/testlib/configs/workload.yml"
"""

        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 2
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""
        self._assertYaml(yaml_input, expected)

    def test_load_entire_workload_param_substitution(self):
        yaml_input = """SchemaVersion: 2018-07-01

LoadConfig:
    Path: "src/testlib/configs/workload.yml"
    Parameters:
      Name: Passes
"""

        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Passes
  Threads: 1
  Phases:
  - Repeat: 2
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""
        self._assertYaml(yaml_input, expected)

    def test_load_entire_workload_nested_param(self):
        yaml_input = """SchemaVersion: 2018-07-01

LoadConfig:
    Path: "src/testlib/configs/workload.yml"
    Parameters:
        Repeat: 3
"""

        # Even though the nested workload defines a "Repeat"
        # parameter, the stored/nested one gets evaluated first.
        expected = """SchemaVersion: '2018-07-01'
Actors:
- Type: Fails
  Name: Fails
  Threads: 1
  Phases:
  - Repeat: 3
    Mode: NoException
- Name: PhaseTimingRecorder
  Type: PhaseTimingRecorder
  Threads: 1
"""
        self._assertYaml(yaml_input, expected)

    def test_load_config_override(self):

        yaml_input = """
SchemaVersion: 2018-07-01
Owner: "@mongodb/server-execution"

LoadConfig:
  Path: src/testlib/configs/LoadConfig.yml
  Parameters:
    MainRandomUUIDFilename: src/testlib/configs/load_config/Gte.yml
    Comment: Overridden comment
"""

        expected = """SchemaVersion: '2018-07-01'
Owner: '@mongodb/server-execution'
Parameters:
- src/testlib/configs/load_config/Gte.yml
Queries:
- OperationName: findOne
  OperationCommand:
    Filter:
      uuid:
        $gte: 1
    Options:
      Comment: Overridden comment
"""
        self._assertYaml(yaml_input, expected)

    def test_load_config_expansion(self):

        yaml_input = """
SchemaVersion: 2018-07-01
Owner: "@mongodb/server-execution"

LoadConfig:
  Path: src/testlib/configs/LoadConfig.yml
  Parameters:
    Comment: Overridden comment
"""

        expected = """SchemaVersion: '2018-07-01'
Owner: '@mongodb/server-execution'
Parameters:
- src/testlib/configs/load_config/Eq.yml
Queries:
- OperationName: findOne
  OperationCommand:
    Filter:
      uuid:
        $eq: 1
    Options:
      Comment: Overridden comment
"""
        self._assertYaml(yaml_input, expected)

    def test_load_config_defaults(self):

        yaml_input = """
SchemaVersion: 2018-07-01
Owner: "@mongodb/server-execution"

LoadConfig:
  Path: src/testlib/configs/LoadConfig.yml
"""

        expected = """SchemaVersion: '2018-07-01'
Owner: '@mongodb/server-execution'
Parameters:
- src/testlib/configs/load_config/Eq.yml
Queries:
- OperationName: findOne
  OperationCommand:
    Filter:
      uuid:
        $eq: 1
    Options:
      Comment: Random UUID
"""
        self._assertYaml(yaml_input, expected)

    def test_override(self):
        yaml_input = """SchemaVersion: 2018-07-01
OverriddenKey: ValueShouldNotExist
NotOverriddenKey: ValueShouldStaySame
HighLevelKey:
  InnerKey:
    InnerInnerKey: Value
    AnotherInnerInnerKey: InnerStaysSame
"""
        yaml_override = """OverriddenKey: ThisIsTheCorrectValue
HighLevelKey:
  InnerKey:
    InnerInnerKey: ChangedInnerValue"""

        with tempfile.TemporaryDirectory() as tmpdirname:
            workload_path = os.path.join(tmpdirname, "workload.yml")
            with open(workload_path, "w") as fp:
                fp.write(yaml_input)

            override_path = os.path.join(tmpdirname, "override.yml")
            with open(override_path, "w") as fp:
                fp.write(yaml_override)

            output = StringIO()
            preprocess.preprocess(
                workload_path=workload_path,
                smoke=False,
                default_uri="FakeUri",
                mongostream_uri="FakeMongostreamUri",
                output_file=output,
                override_file_path=override_path,
            )

            # Even though the nested workload defines a "Repeat"
            # parameter, the stored/nested one gets evaluated first.
            expected = """# This file was generated by running the Genny preprocessor on the workload workload.yml
Clients:
  Default:
    Type: mongo
    QueryOptions:
      maxPoolSize: 100
    URI: FakeUri
  Stream:
    Type: mongostream
    QueryOptions:
      maxPoolSize: 100
    URI: FakeMongostreamUri
SchemaVersion: '2018-07-01'
OverriddenKey: ThisIsTheCorrectValue
NotOverriddenKey: ValueShouldStaySame
HighLevelKey:
  InnerKey:
    InnerInnerKey: ChangedInnerValue
    AnotherInnerInnerKey: InnerStaysSame
"""
        output.seek(0)
        self.assertEqual(output.read(), expected)

    def test_numexpr_no_dict(self):
        yaml_input = """SchemaVersion: 2018-07-01
Test: {^NumExpr: {withExpression: "10 - 50"}}
"""
        expected = """SchemaVersion: '2018-07-01'
Test: -40
"""
        self._assertYaml(yaml_input, expected)

    def test_numexpr_with_dict(self):
        yaml_input = """SchemaVersion: 2018-07-01
Test: {^NumExpr: {withExpression: "a - b", andValues: {a: 100, b: 25}}}
"""

        expected = """SchemaVersion: '2018-07-01'
Test: 75
"""
        self._assertYaml(yaml_input, expected)

    def test_numexpr_with_dict_parameter(self):
        yaml_input = """SchemaVersion: 2018-07-01
Param1: &Param1 {^Parameter: {Name: "Name1", Default: 100}}
Test: {^NumExpr: {withExpression: "a - b", andValues: {a: *Param1, b: 25}}}
"""

        expected = """SchemaVersion: '2018-07-01'
Param1: 100
Test: 75
"""
        self._assertYaml(yaml_input, expected)

    def test_numexpr_within_document_generator_expression(self):
        yaml_input = """SchemaVersion: 2018-07-01
Param1: &Param1 {^Parameter: {Name: "Name1", Default: 3}}
Document: {^FastRandomString:{length:{^NumExpr: {withExpression: "a + 3", andValues: {a: *Param1}}}}}
"""

        expected = """SchemaVersion: '2018-07-01'
Param1: 3
Document:
  ^FastRandomString:
    length: 6
"""
        self._assertYaml(yaml_input, expected)

    def test_numexpr_non_string_expr_throws(self):
        yaml_input = """SchemaVersion: 2018-07-01
Test: {^NumExpr: {withExpression: 1}}
"""
        self._assertParseException(yaml_input)

    def test_numexpr_with_dict_string_value_throws(self):
        yaml_input = """SchemaVersion: 2018-07-01
Param1: &Param1 {^Parameter: {Name: "Name1", Default: 100}}
Test: {^NumExpr: {withExpression: "a - b", andValues: {a: *Param1, b: "25"}}}
"""
        self._assertParseException(yaml_input)

    def test_formatstring_with_preprocessable_args(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^PreprocessorFormatString:
    format: "%s %04d"
    withArgs:
    - Test
    - ^NumExpr:
        withExpression: "a * b"
        andValues:
          a: 3
          b: 4
"""

        expected = """SchemaVersion: '2018-07-01'
Document: Test 0012
"""

        self._assertYaml(yaml_input, expected)

    def test_formatstring_with_invalid_args(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
- ^PreprocessorFormatString:
    format: "%s %04d"
    withArgs:
    - Test
    - [0, 0, 1, 2]
- ^PreprocessorFormatString:
    format: "%04d"
    withArgs:
    - ^Array:
        number: 4
        of: 1
"""

        self._assertParseException(yaml_input)

    def test_formatstring_with_invalid_arg_type(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^PreprocessorFormatString:
    format: "%s %04d"
    withArgs:
    - Test
    - "0012"
"""

        self._assertParseException(yaml_input)

    def test_flatten_array_valid_type_arrays(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^FlattenOnce:
  - 1
  - "2"
  - [3, "4"]
  - ^FlattenOnce: [5, [6, "7", ["8", 9, [], [], [1], [[[1]], [False], [""]]]]]
"""

        expected = """SchemaVersion: '2018-07-01'
Document:
- 1
- '2'
- 3
- '4'
- 5
- 6
- '7'
- - '8'
  - 9
  - []
  - []
  - - 1
  - - - - 1
    - - false
    - - ''
"""

        self._assertYaml(yaml_input, expected)

    def test_flatten_array_valid_type_dict(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^FlattenOnce:
    1: 2
    3: 4
    5: 6
"""

        expected = """SchemaVersion: '2018-07-01'
Document:
- 1
- 3
- 5
"""

        self._assertYaml(yaml_input, expected)

    def test_flatten_array_invalid_type_int(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^FlattenOnce: 1
"""

        self._assertParseException(yaml_input)

    def test_flatten_array_valid_type_str(self):
        yaml_input = """SchemaVersion: 2018-07-01
Document:
  ^FlattenOnce: "12345"
"""

        expected = """SchemaVersion: '2018-07-01'
Document:
- '1'
- '2'
- '3'
- '4'
- '5'
"""

        self._assertYaml(yaml_input, expected)

    def test_clienturi_reference(self):
        yaml_input: str = (
            "Clients:\n"
            "  Default:\n"
            "    URI: mongodb://mongod:27017\n"
            "  Stream:\n"
            "    URI: mongodb://mongostream:27017\n"
            "SchemaVersion: 2018-07-01\n"
            "MongoClientURI: {^ClientURI: { Name: 'Default' }}\n"
            "MongostreamClientURI: {^ClientURI: { Name: 'Stream' }}"
        )
        expected: str = (
            "Clients:\n"
            "  Default:\n"
            "    URI: mongodb://mongod:27017\n"
            "  Stream:\n"
            "    URI: mongodb://mongostream:27017\n"
            "SchemaVersion: '2018-07-01'\n"
            "MongoClientURI: mongodb://mongod:27017\n"
            "MongostreamClientURI: mongodb://mongostream:27017\n"
        )

        self._assertYaml(yaml_input, expected)
