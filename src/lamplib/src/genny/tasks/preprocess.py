import yaml
import structlog

SLOG = structlog.get_logger(__name__)


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
