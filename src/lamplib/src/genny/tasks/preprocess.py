from enum import Enum
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
        for scope in self._scopes:
            if name in scope:
                stored_value: Context.ContextValue = scope[name]
                actual_type = stored_value.type
                if actual_type != expected_type:
                    msg = (f"Type mismatch for node named {name}. Expected {expected_type.name}"
                           f" but received {stored_value.type.name}")
                    raise ParseException(msg)
                return stored_value.value
        # TODO: Create special value?
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

    def __init__(self):
        #TODO: Calculate phase path, handle strings yamls?
        self._phase_config_path = ""
        self._context = Context()

    # TODO: Add smoke mode and yaml source?
    def parse(self, filename):
        with self._context.enter():
            workload = None
            with open(filename) as file:
                workload = yaml.full_load(file)

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
            out = self._replaceParam(value)
        elif (key == "ActorTemplates"):
            self._parse_templates(value)
        elif (key == "ActorFromTemplate"):
            out[key] = self._recursive_parse(value)
            pass
            #out = parseInstance(value)
        elif (key == "OnlyActiveInPhases"):
            out = self._parse_only_in(value)
        elif (key == "ExternalPhaseConfig"):
            out[key] = self._recursive_parse(value)
            pass
            #external = parseExternal(value);
            # Merge the external node with the any other parameters specified
            # for this node like "Repeat" or "Duration".

            #for (externalKvp : external) {
            #    if (!out[externalKvp.first])
            #        out[externalKvp.first] = externalKvp.second;
            #}
        else:
            out[key] = self._recursive_parse(value)
        return out

    def _replaceParam(self, input):
        #if (!input["Name"] || !input["Default"]) {
        if "Name" not in input or "Default" not in input:
            msg = ("Invalid keys for '^Parameter', please set 'Name' and 'Default'"
                   f" in following node: {input}")
            raise ParseException(msg)

        name = input["Name"]
        # The default value is mandatory.
        defaultVal = input["Default"]

        # Nested params are ignored for simplicity.
        paramVal = self._context.get(name, ContextType.Parameter)
        if (paramVal):
            return paramVal
        else:
            return defaultVal

    def _parse_templates(self, templates):
        for template_node in templates:
            self._context.insert(template_node["TemplateName"], template_node["Config"],
                                 ContextType.ActorTemplate)


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
