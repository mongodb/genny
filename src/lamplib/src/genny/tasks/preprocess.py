from enum import Enum
from collections import namedtuple

import yaml
import structlog

SLOG = structlog.get_logger(__name__)


class ParseException(Exception):
    pass


# TODO: Can do this better now in python
class Type(Enum):
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
    #using ContextValue = std::pair<YAML::Node, Type>;
    #using Scope = std::map<std::string, std::pair<YAML::Node, Type>>;

    ContextValue = namedtuple('ContextValue', ['node', 'type'])

    def __init__(self):
        self._scopes = []

    def get(self, name: str, expected_type: Type):
        for scope in self._scopes:
            if name in scope:
                stored_value = scope[name]
                actual_type = stored_value.type
                if actual_type != expected_type:
                    msg = (f"Type mismatch for node named {name}. Expected {expected_type.name}"
                           f" but received {stored_value.type.name}")
                    raise ParseException(msg)
                return stored_value.value
        return None

    #void insert(const std::string&, const YAML::Node&, const Type& type);

    # Insert all the values of a node, assuming they are of a type.
    #void insert(const YAML::Node&, const Type& type);

    #/**
    # * Opens a scope, closes it when exiting scope.
    # */
    #struct ScopeGuard {
    #    ScopeGuard(Context& context) : _context{context} {
    #        // Emplace (push) a new Context onto the stack.
    #        _context._scopes.emplace_back();
    #    }
    #    ~ScopeGuard() {
    #        _context._scopes.pop_back();
    #    }

    #private:
    #    Context& _context;
    #};
    #ScopeGuard enter() {
    #    return ScopeGuard(*this);
    #}

#private:
    #// Use vector as a stack since std::stack isn't iterable.
    #std::vector<Scope> _scopes;
#};


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
