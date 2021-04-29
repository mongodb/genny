from enum import Enum
from collections import namedtuple
from contextlib import AbstractContextManager

import yaml
import structlog

SLOG = structlog.get_logger(__name__)


class ParseException(Exception):
    pass


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


ContextValue = namedtuple('ContextValue', ['node', 'type'])


class Context(object):
    """
    Manages scoped context for stored values.

    Contexts are helpers for the workload parser.
    """

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
                stored_value: ContextValue = scope[name]
                actual_type = stored_value.type
                if actual_type != expected_type:
                    msg = (f"Type mismatch for node named {name}. Expected {expected_type.name}"
                           f" but received {stored_value.type.name}")
                    raise ParseException(msg)
                return stored_value.value
        return None

    def insert(self, name: str, val, val_type: ContextType):
        """Insert a value with a name into the current scope."""
        self._scopes[-1][name] = ContextValue(val, val_type)

    def insert(self, node: dict, val_type: ContextType):
        """Insert all the values of a node, assuming they are of a type."""
        if not isinstance(node, dict):
            msg = (f"Invalid context storage of node: {node}."
                   ". Please ensure this node is a map rather than a sequence.")
            raise ParseException(msg)
        }

        for key, val in node.items():
            self.insert(key, val, val_type)

    # TODO: Use the contextlib decorator maybe?
    def open():
        """Open a new scope, for use in a context."""
        return ScopeManager(self)

    class ScopeManager(AbstractContextManager):
        """Class for managing a single scope."""

        def __init__(self, context):
            """Initialize ScopeManager with the context object it participates in."""
            self._context = context

        def __enter__(self):
            scope = {}
            self._context._scopes.append(scope)
            return scope

        def __exit__(self):
            self._context._scopes.pop()

def evaluate(workload_path: str):
    output_file = None
    output = preprocess_file(workload_path)
    output_logger = structlog.PrintLogger(output_file)
    output_logger.msg(output)


def preprocess_file(filename):
    doc = None
    with open(filename) as file:
        doc = yaml.full_load(file)

    doc = recursive_parse(doc)
    output = yaml.dump(doc, sort_keys=False)
    return output


def recursive_parse(node):
    out = None

    if isinstance(node, dict):
        for key, value in node.items():
            out = preprocess(key, value, out)
    elif isinstance(node, list):
        out = []
        for val in node:
            out.append(recursive_parse(val))
    else:
        out = node

    return out


def preprocess(key, value, out):
    if out is None:
        out = {}

    if (key == "OnlyActiveInPhases"):
        out = parse_only_in(value)
    else:
        out[key] = recursive_parse(value)
    return out


def parse_only_in(onlyIn):
    out = []
    nop = {}
    nop["Nop"] = True
    max = recursive_parse(onlyIn["NopInPhasesUpTo"])
    for i in range(max+1):
        isActivePhase = False
        for activePhase in recursive_parse(onlyIn["Active"]):
            if (activePhase == i):
                out.append(recursive_parse(onlyIn["PhaseConfig"]))
                isActivePhase = True
                break
        if not isActivePhase:
            out.append(nop)
    return out
