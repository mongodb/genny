import os
import sys
from enum import Enum
from pathlib import Path
from collections import namedtuple
from contextlib import AbstractContextManager

import yaml
import structlog

SLOG = structlog.get_logger(__name__)


class ParseException(Exception):
    pass

def evaluate(workload_path: str):
    output_file = None
    parser = WorkloadParser()
    output = parser.parse(workload_path)
    output_logger = structlog.PrintLogger(output_file)
    output_logger.msg(output)


# TODO: Can do this better now in python
class ContextType(Enum):
    """
    Values stored in a Context are tagged with a type to ensure
    they aren't used incorrectly.

    If updating, please add to typeNames in workload_parsers.cpp
    Enums don't have good string-conversion or introspection :(
    """
    Parameter = 1,
    ActorTemplate = 2,


class Context(object):
    """
    Manages scoped context for stored values.

    Contexts are helpers for the workload parser.
    """

    ContextValue = namedtuple('ContextValue', ['value', 'type'])

    def __init__(self):
        """Initialize Context."""
        # self._scopes is a list where each element is a level
        # in the scope hierarchy. Each element is a map of names
        # to stored ContextValues.
        self._scopes = []

    def get(self, name: str, expected_type: ContextType):
        """Retrieve a node with a given name and context type."""
        for scope in reversed(self._scopes):
            if name in scope:
                stored_value: Context.ContextValue = scope[name]
                actual_type = stored_value.type
                if actual_type != expected_type:
                    msg = (f"Type mismatch for node named {name}. Expected {expected_type.name}"
                           f" but received {stored_value.type.name}")
                    raise ParseException(msg)
                return stored_value.value
        return None

    def insert(self, name: str, val, val_type: ContextType):
        """Insert a value with a name into the current scope."""
        self._scopes[-1][name] = Context.ContextValue(val, val_type)

    def insert_all(self, node: dict, val_type: ContextType):
        """Insert all the values of a node, assuming they are of a type."""
        if not isinstance(node, dict):
            msg = (f"Invalid context storage of node: {node}."
                   ". Please ensure this node is a map rather than a sequence.")
            raise ParseException(msg)

        for key, val in node.items():
            self.insert(key, val, val_type)

    class ScopeManager(AbstractContextManager):
        """Class for managing a single scope."""

        def __init__(self, context):
            """Initialize ScopeManager with the context object it participates in."""
            self._context = context

        def __enter__(self):
            scope = {}
            self._context._scopes.append(scope)
            return scope

        def __exit__(self, exc_type, exc_value, exc_traceback):
            self._context._scopes.pop()

    # TODO: Use the contextlib decorator maybe?
    def enter(self):
        """Enter a new scope, for use in a context manager."""
        return Context.ScopeManager(self)

class WorkloadParser(object):

    class YamlSource(Enum):
        File = 1,
        String = 2


    def __init__(self):
        #TODO: Handle strings yamls?
        self._phase_config_path = ""
        self._context = Context()

    # TODO: Add smoke mode and corresponding yaml source?
    def parse(self, yaml_input, source=YamlSource.File, path=""):
        
        with self._context.enter():
            if source == WorkloadParser.YamlSource.File:
                workload = _load_file(yaml_input)
                path = Path(yaml_input)
                self._phase_config_path = path.parent.absolute()
            elif source == WorkloadParser.YamlSource.String:
                workload = yaml.safe_load(yaml_input)
                if path == "":
                    raise ParseException("Must specify path for string yaml sources.")
                self._phase_config_path = path
            else:
                raise ParseException(f"Invalid yaml source type {source}.")
            doc = self._recursive_parse(workload)
            parsed = yaml.dump(doc, sort_keys=False)
            return parsed

    def _recursive_parse(self, node):
        if isinstance(node, dict):
            out = {}
            for key, value in node.items():
                out = self._preprocess(key, value, out)
        elif isinstance(node, list):
            out = []
            for val in node:
                out.append(self._recursive_parse(val))
        else:
            out = node

        return out


    def _preprocess(self, key, value, out):
        if (key == "^Parameter"):
            out = self._replace_param(value)
        elif (key == "ActorTemplates"):
            self._parse_templates(value)
        elif (key == "ActorFromTemplate"):
            out = self._parse_instance(value)
        elif (key == "OnlyActiveInPhases"):
            out = self._parse_only_in(value)
        elif (key == "ExternalPhaseConfig"):
            external = self._parse_external(value)

            # Merge the external node with the any other parameters specified
            # for this node like "Repeat" or "Duration".
            for key in external:
                if key not in out:
                    out[key] = external[key]
        else:
            out[key] = self._recursive_parse(value)
        return out

    def _replace_param(self, input):
        if "Name" not in input or "Default" not in input:
            msg = ("Invalid keys for '^Parameter', please set 'Name' and 'Default'"
                   f" in following node: {input}")
            raise ParseException(msg)

        name = input["Name"]
        
        # The default value is mandatory.
        defaultVal = input["Default"]

        # Nested params are ignored for simplicity.
        paramVal = self._context.get(name, ContextType.Parameter)
        if paramVal is not None:
            print("returning paramVal: ", paramVal)
            return paramVal
        else:
            return defaultVal

    def _parse_templates(self, templates):
        for template_node in templates:
            self._context.insert(template_node["TemplateName"], template_node["Config"],
                                 ContextType.ActorTemplate)

    def _parse_instance(self, instance):
        actor = {}

        with self._context.enter():
            templateNode = self._context.get(instance["TemplateName"], ContextType.ActorTemplate)
            if templateNode is None:
                name = instance["TemplateName"]
                msg = f"Expected template named {name} but could not be found."
                raise ParseException(msg)
            self._context.insert_all(instance["TemplateParameters"], ContextType.Parameter)
            actor = self._recursive_parse(templateNode)
        return actor


    def _parse_only_in(self, onlyIn):
        out = []
        nop = {}
        nop["Nop"] = True
        max = self._recursive_parse(onlyIn["NopInPhasesUpTo"])
        for i in range(max+1):
            isActivePhase = False
            for activePhase in self._recursive_parse(onlyIn["Active"]):
                if (activePhase == i):
                    out.append(self._recursive_parse(onlyIn["PhaseConfig"]))
                    isActivePhase = True
                    break
            if not isActivePhase:
                out.append(nop)
        return out

    def _parse_external(self, external):
        with self._context.enter():
            keysSeen = 0

            if ("Path" not in external):
                msg = f"Missing the `Path` top-level key in your external phase configuration: {external}"
                raise ParseException(msg)

            path = external["Path"]
            keysSeen += 1

            path = os.path.join(self._phase_config_path, path)

            if not os.path.isfile(path):
                msg = (f"Invalid path to external PhaseConfig: {path}"
                        ". Please ensure your workload file is placed in 'workloads/[subdirectory]/' and the "
                        "'Path' parameter is relative to the 'phases/' directory")
                raise ParseException(path)

            replacement = _load_file(path)

            if "PhaseSchemaVersion" not in replacement:
                raise ParseException(
                    "Missing the `PhaseSchemaVersion` top-level key in your external phase "
                    "configuration")

            # Python will parse the schema version as a datetime.
            phase_schema_version = str(replacement["PhaseSchemaVersion"])
            if phase_schema_version != "2018-07-01":
                msg = (f"Invalid phase schema version: {phase_schema_version}"
                       ". Please ensure the schema for your external phase config is valid and the "
                       "`PhaseSchemaVersion` top-level key is set correctly")
                raise ParseException(msg)

            # Delete the schema version instead of adding it to `keysSeen`.
            del replacement["PhaseSchemaVersion"]

            if "Parameters" in external:
                keysSeen += 1
                self._context.insert_all(external["Parameters"], ContextType.Parameter)

            if "Key" in external:
                keysSeen += 1
                key = external["Key"]
                if key not in replacement:
                    msg = f"Could not find top-level key: {key} in phase config YAML file: {path}"
                    raise ParseException(msg)
                replacement = replacement[key]

            if (len(external) != keysSeen):
                msg = ("Invalid keys for 'External'. Please set 'Path' and if any, 'Parameters' in the YAML "
                       f"file: {path} with the following content: {external}")
                raise ParseException(msg)

            return self._recursive_parse(replacement)

def _load_file(source):
    try:
        with open(source) as file:
            workload = yaml.safe_load(file)
            return workload
    except:
        SLOG.error(f"Error loading yaml from {source}: {sys.exc_info()[0]}")
        raise
