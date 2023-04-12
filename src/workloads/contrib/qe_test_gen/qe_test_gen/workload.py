import math
from jinja2 import Environment, PackageLoader, select_autoescape

DOCUMENT_COUNT = 100000


class LoadPhase:
    def __init__(self, env):
        self.env = env

    def context(self):
        return {}

    def generate(self):
        template = self.env.get_template("load_phase.jinja2")
        return template


class UpdatePhase:
    def __init__(self, env, query, update):
        self.env = env
        self.queries = query
        self.updates = update

    def context(self):
        return {"count": len(self.queries) * len(self.updates), "queries": self.queries, "updates": self.updates}

    def generate(self):
        template = self.env.get_template("update_phase.jinja2")
        return template


class Workload:
    def __init__(self, testName, description, coll, env, ef, cf, tc, phaseFactory):
        self.env = env
        self.testName = testName
        self.description = description
        # self.testName =  f"UpdateOnly-{ex['name']}-{subExperiment}-{cf}-{tc}"
        self.contentionFactor = cf
        self.encryptedFields = ef
        self.threadCount = tc
        self.collectionName = coll
        self.documentCount = DOCUMENT_COUNT

        if coll == "blimit":
            self.documentCount = 1_000_000

        self.parser = phaseFactory

    def asContext(self):
        phases = self.parser.makePhases(self.env)

        context = {
            "testName": self.testName,
            "description": self.description,
            "contentionFactor": self.contentionFactor,
            "encryptedFields": self.encryptedFields,
            "threadCount": self.threadCount,
            "collectionName": self.collectionName,
            "iterationsPerThread": math.floor(self.documentCount / self.threadCount),
            "maxPhase": len(phases) - 1,
            "shouldAutoRun": True,
            "phases": phases,
        }

        return context
