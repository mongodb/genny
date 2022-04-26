import os
import sys
import tempfile
import datetime
from enum import Enum
from pathlib import Path
from collections import namedtuple
from contextlib import AbstractContextManager

from omegaconf import OmegaConf
import yaml
import structlog
import numexpr

SLOG = structlog.get_logger(__name__)
# Cannot be in the default config because yaml merges overwrite lists instead of appending.
GENNY_INTERNAL = {"Name": "PhaseTimingRecorder", "Type": "PhaseTimingRecorder", "Threads": 1}
# If this gets any bigger, we should consider a yaml file.
DEFAULT_CONFIG = {
    # We go ahead and set the default pool with a 100 size so that
    # later URI injection can configure it. It's okay if the Default
    # pool isn't actually used in a workload, since they're constructed
    # lazily.
    "Clients": {"Default": {"QueryOptions": {"maxPoolSize": 100}}}
}


class ParseException(Exception):
    pass


def evaluate(
    workload_path: str, default_uri: str, smoke: bool, output: str, override_file_path=None
):
    """CLI-friendly wrapper for preprocess."""
    if output is not None:
        with open(output, "w") as f:
            preprocess(
                workload_path=workload_path,
                default_uri=default_uri,
                smoke=smoke,
                output_file=f,
                override_file_path=override_file_path,
            )
    else:
        preprocess(
            workload_path=workload_path,
            default_uri=default_uri,
            smoke=smoke,
            override_file_path=override_file_path,
        )


# It's weird to mix our custom preprocessor with OmegaConf.
# Future work can replace it with OmegaConf resolvers and interpolation.
def preprocess(
    workload_path: str,
    smoke: bool,
    default_uri: str,
    output_file=sys.stdout,
    override_file_path=None,
):
    """Evaluate a workload and output it to a file (or stdout)."""
    mode = _ParseMode.Smoke if smoke else _ParseMode.Normal

    # First, apply the workload yaml over the defaults.
    conf = OmegaConf.load(workload_path)
    OmegaConf.create(DEFAULT_CONFIG)
    conf = OmegaConf.unsafe_merge(DEFAULT_CONFIG, conf)

    # Second, apply any overrides.
    if override_file_path is not None:
        overrides = OmegaConf.load(override_file_path)
        conf = OmegaConf.unsafe_merge(conf, overrides)

    # Third, use preprocessor on the merged config.
    with tempfile.NamedTemporaryFile() as fp:
        OmegaConf.save(config=conf, f=fp.name)
        parser = _WorkloadParser()
        path = Path(workload_path)
        raw_parsed = parser.parse(fp.name, path=path, parse_mode=mode, default_uri=default_uri)
        conf = OmegaConf.create(raw_parsed)

    output_logger = structlog.PrintLogger(output_file)
    output_logger.msg(
        "# This file was generated by running the Genny preprocessor on the workload "
        + os.path.basename(workload_path)
    )
    OmegaConf.save(config=conf, f=output_file)


class _ContextType(Enum):
    """
    Values stored in a Context are tagged with a type to ensure
    they aren't used incorrectly.
    """

    Parameter = (1,)
    ActorTemplate = (2,)


class _ParseMode(Enum):
    Normal = (1,)
    Smoke = (2,)


class _Context(object):
    """
    Manages scoped context for stored values.

    Contexts are helpers for the workload parser.
    """

    ContextValue = namedtuple("ContextValue", ["value", "type"])

    def __init__(self):
        """Initialize Context."""
        # self._scopes is a list where each element is a level
        # in the scope hierarchy. Each element is a map of names
        # to stored ContextValues.
        self._scopes = []

    def get(self, name: str, expected_type: _ContextType):
        """Retrieve a node with a given name and context type."""
        for scope in reversed(self._scopes):
            if name in scope:
                stored_value: _Context.ContextValue = scope[name]
                actual_type = stored_value.type
                if actual_type != expected_type:
                    msg = (
                        f"Type mismatch for node named {name}. Expected {expected_type.name}"
                        f" but received {stored_value.type.name}"
                    )
                    raise ParseException(msg)
                return stored_value.value
        return None

    def insert(self, name: str, val, val_type: _ContextType):
        """Insert a value with a name into the current scope."""
        self._scopes[-1][name] = _Context.ContextValue(val, val_type)

    def insert_all(self, node: dict, val_type: _ContextType):
        """Insert all the values of a node, assuming they are of a type."""
        if not isinstance(node, dict):
            msg = (
                f"Invalid context storage of node: {node}."
                ". Please ensure this node is a map rather than a sequence."
            )
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

    def enter(self):
        """Enter a new scope, for use in a context manager."""
        return _Context.ScopeManager(self)


class _WorkloadParser(object):
    """Parses/preprocesses workloads, stores state while doing so."""

    class YamlSource(Enum):
        """Where the yaml data is read from."""

        File = (1,)
        String = 2

    def __init__(self):
        """Initialize WorkloadParser."""
        self._phase_config_path = ""
        self._context = _Context()

    def parse(
        self,
        yaml_input,
        default_uri,
        source=YamlSource.File,
        path="",
        parse_mode=_ParseMode.Normal,
    ):
        """Parse the yaml input, assumed to be a file by default."""

        if path == "":
            raise ParseException("Must specify path of original yaml for parser.")

        self._default_uri = default_uri
        with self._context.enter():
            if source == _WorkloadParser.YamlSource.File:
                workload = _load_file(yaml_input)
                self._phase_config_path = path.parent.absolute()
            elif source == _WorkloadParser.YamlSource.String:
                workload = yaml.safe_load(yaml_input)
                self._phase_config_path = path
            else:
                raise ParseException(f"Invalid yaml source type {source}.")
            doc = self._recursive_parse(workload)

            if parse_mode == _ParseMode.Normal:
                pass
            elif parse_mode == _ParseMode.Smoke:
                doc = _smoke_convert(doc)
            return doc

    def _recursive_parse(self, node):
        if isinstance(node, dict):
            out = {}
            for key, value in node.items():
                out = self._preprocess(key, value, out)
        elif isinstance(node, list):
            out = []
            for val in node:
                out.append(self._recursive_parse(val))
        elif isinstance(node, datetime.date):
            out = str(node)
        else:
            out = node

        return out

    def _preprocess(self, key, value, out):
        if key == "^Parameter":
            out = self._replace_param(value)
        elif key == "^NumExpr":
            out = self._replace_numexpr(value)
        elif key == "ActorTemplates":
            self._parse_templates(value)
        elif key == "ActorFromTemplate":
            out = self._parse_instance(value)
        elif key == "Actors":
            out["Actors"] = self._parse_actors(value)
        elif key == "Clients":
            out["Clients"] = self._parse_clients(value)
        elif key == "OnlyActiveInPhases":
            out = self._parse_only_in(value)
        elif key == "LoadConfig":
            loaded_config = self._parse_load_config(value)

            if isinstance(loaded_config, dict):
                # Merge the loaded node with the any other parameters specified
                # for this node like "Repeat" or "Duration".
                for key in loaded_config:
                    if key not in out:
                        out[key] = loaded_config[key]
            else:
                out = loaded_config
        else:
            out[key] = self._recursive_parse(value)
        return out

    def _replace_param(self, input):
        if "Name" not in input or "Default" not in input:
            msg = (
                "Invalid keys for '^Parameter', please set 'Name' and 'Default'"
                f" in following node: {input}"
            )
            raise ParseException(msg)

        name = input["Name"]

        # The default value is mandatory.
        defaultVal = input["Default"]

        # Nested params are evaluated.
        paramVal = self._context.get(name, _ContextType.Parameter)
        if paramVal is not None:
            return self._recursive_parse(paramVal)
        else:
            return self._recursive_parse(defaultVal)

    def _replace_numexpr(self, input):
        if "Expr" not in input:
            msg = "Invalid keys for '^NumExpr', please set 'Expr'" f" in following node: {input}"
            raise ParseException(msg)

        if type(input["Expr"]) != str:
            msg = "Invalid value for 'Expr', which must be a string," f" in following node: {input}"
            raise ParseException(msg)

        parsed_values = {}  # Pass empty dict to avoid yaml to access context of this function
        if "Dict" in input:
            input_values = input["Dict"]
            parsed_values = self._recursive_parse(input_values)
            if not all(type(value) == int or type(value) == float for value in parsed_values.values()):
                msg = (
                    "Invalid values for 'Dict' in '^NumExpr', only numerical values are allowed.\n"
                    f"Node source: {input_values}\n"
                    f"Node eval: {parsed_values}\n"
                )
                raise ParseException(msg)

        try:
            return numexpr.evaluate(input["Expr"], parsed_values).tolist()
        except KeyError as e:
            msg = (
                "Key used in 'Expr' not found in 'Dict'\n"
                f"Node source: {input}\n"
                f"Faulting key: {e}\n"
            )
            raise ParseException(msg)
        except Exception as e:
            msg = (
                "Failure trying to parse ^NumExpr node.\n"
                f"Node source: {input}\n"
                f"Exception: {e}\n"
            )
            raise ParseException(msg)

    def _parse_templates(self, templates):
        for template_node in templates:
            self._context.insert(
                template_node["TemplateName"], template_node["Config"], _ContextType.ActorTemplate
            )

    def _parse_actors(self, actors):
        actor_list = self._recursive_parse(actors)
        names = [actor["Name"] for actor in actor_list]
        if "PhaseTimingRecorder" not in names:
            actor_list.append(self._recursive_parse(GENNY_INTERNAL))
        return actor_list

    def _parse_clients(self, clients):
        clients_dict = self._recursive_parse(clients)
        for _, client in clients_dict.items():
            client.setdefault("URI", self._default_uri)
        return clients_dict

    def _parse_instance(self, instance):
        actor = {}

        with self._context.enter():
            templateNode = self._context.get(instance["TemplateName"], _ContextType.ActorTemplate)
            if templateNode is None:
                name = instance["TemplateName"]
                msg = f"Expected template named {name} but could not be found."
                raise ParseException(msg)

            paramNode = instance["TemplateParameters"]
            if paramNode is not None and not isinstance(paramNode, dict):
                msg = (
                    f"Invalid context storage of node: {paramNode}."
                    ". Please ensure this node is a map rather than a sequence."
                )
                raise ParseException(msg)

            for key, val in instance["TemplateParameters"].items():
                self._context.insert(key, self._recursive_parse(val), _ContextType.Parameter)

            actor = self._recursive_parse(templateNode)
        return actor

    def _parse_only_in(self, onlyIn):
        out = []
        nop = {}
        nop["Nop"] = True
        max = self._recursive_parse(onlyIn["NopInPhasesUpTo"])
        for i in range(max + 1):
            isActivePhase = False
            for activePhase in self._recursive_parse(onlyIn["Active"]):
                if activePhase == i:
                    out.append(self._recursive_parse(onlyIn["PhaseConfig"]))
                    isActivePhase = True
                    break
            if not isActivePhase:
                out.append(nop)
        return out

    def _parse_load_config(self, load_config):
        with self._context.enter():
            keysSeen = 0

            if "Path" not in load_config:
                msg = f"Missing the `Path` top-level key in your loadable configuration: {load_config}"
                raise ParseException(msg)

            out = {}
            path = load_config["Path"]
            out = self._preprocess("Path", path, out)
            path = out["Path"]

            keysSeen += 1

            path = os.path.join(self._phase_config_path, path)

            if not os.path.isfile(path):
                msg = (
                    f"Invalid path to loadable config: {path}"
                    ". Please ensure your workload file is placed in 'workloads/[subdirectory]/' and the "
                    "'Path' parameter is relative to the 'phases/' directory"
                )
                raise ParseException(msg)

            replacement = _load_file(path)

            if "SchemaVersion" not in replacement:
                raise ParseException(
                    "Missing the `SchemaVersion` top-level key in your loadable " "configuration"
                )

            # Python will parse the schema version as a datetime.
            loadable_schema_version = str(replacement["SchemaVersion"])
            if loadable_schema_version != "2018-07-01":
                msg = (
                    f"Invalid phase schema version: {loadable_schema_version}"
                    ". Please ensure the schema for your loadable config is valid and the "
                    "`SchemaVersion` top-level key is set correctly"
                )
                raise ParseException(msg)

            # Delete the schema version instead of adding it to `keysSeen`.
            del replacement["SchemaVersion"]

            if "Parameters" in load_config:
                keysSeen += 1
                paramNode = load_config["Parameters"]
                if not isinstance(paramNode, dict):
                    msg = (
                        f"Invalid context storage of node: {paramNode}."
                        ". Please ensure this node is a map rather than a sequence."
                    )
                    raise ParseException(msg)

                for key, val in load_config["Parameters"].items():
                    self._context.insert(key, self._recursive_parse(val), _ContextType.Parameter)

            if "Key" in load_config:
                keysSeen += 1
                key = load_config["Key"]
                if key not in replacement:
                    msg = (
                        f"Could not find top-level key: {key} in loadable config YAML file: {path}"
                    )
                    raise ParseException(msg)
                replacement = replacement[key]

            if len(load_config) != keysSeen:
                msg = (
                    "Invalid keys for 'LoadConfig'. Please set 'Path' and if any, 'Parameters' in the YAML "
                    f"file: {path} with the following content: {external}"
                )
                raise ParseException(msg)

            return self._recursive_parse(replacement)


def _smoke_convert(workload_root):
    """
    Convert a workload YAML into a version for smoke test where every phase
    of every actor runs with Repeat: 1
    """

    actors_out = []

    # Convert keywords in the "Actors" block.
    for actor in workload_root["Actors"]:
        actor_out = _convert_obj_for_smoke(actor)
        phases_out = []

        # Convert keywords in the "Phases" block.
        for phase in actor_out["Phases"]:
            phases_out.append(_convert_obj_for_smoke(phase))

        actor_out["Phases"] = phases_out
        actors_out.append(actor_out)

    workload_root["Actors"] = actors_out

    return workload_root


def _convert_obj_for_smoke(in_node):
    out = {}
    for key, value in in_node.items():
        if key == "Duration" or key == "Repeat":
            out["Repeat"] = 1
        elif key == "GlobalRate" or key == "SleepBefore" or key == "SleepAfter":
            # Ignore those keys in smoke tests.
            pass
        else:
            out[key] = value
    return out


def _load_file(source):
    try:
        with open(source) as file:
            workload = yaml.safe_load(file)
            return workload
    except:
        SLOG.error(f"Error loading yaml from {source}: {sys.exc_info()[0]}")
        raise
