from jinja2 import Environment, Template, PackageLoader, select_autoescape
from faker import Faker
from random import Random
import math
import string
from abc import ABC, abstractmethod
import re


env = Environment(
    loader=PackageLoader("qe_test_gen"),
    variable_start_string="<<",
    variable_end_string=">>",
    trim_blocks=True,
    lstrip_blocks=True,
    autoescape=select_autoescape(),
)

template = env.get_template("medical_datamap.jinja2")


class Snippet:
    def __init__(self, template: Template, context):
        self._template = template
        self._context = context

    def context(self):
        return self._context

    def generate(self):
        return self._template


class Distribution(ABC):
    def __init__(self, field_name: str, contention_factor: int):
        self.field_name = field_name
        self.contention_factor = contention_factor

    @abstractmethod
    def emit_generator():
        pass

    @abstractmethod
    def emit_targetter():
        pass

    @abstractmethod
    def emit_values():
        pass


class ExplicitDistribution(Distribution):
    def __init__(self, field_name: str, contention_factor: int, map: dict[str, int]):
        super(ExplicitDistribution, self).__init__(field_name, contention_factor)
        self.map = map

    def emit_generator(self):
        template = env.get_template("explicit_distribution.jinja2")
        return Snippet(template, {"field_name": self.field_name, "map": self.map})

    def emit_targetter(self):
        return (
            f'{{^ChooseFromDataset: {{"path": "./src/genny/src/workloads/contrib/qe_test_gen/{self.field_name}.txt"}}}}'
        )

    def emit_values(self):
        return iter(self.map)


states = ExplicitDistribution(
    "states",
    16,
    {
        "California": 118000,
        "Texas": 86030,
        "Florida": 64280,
        "New York": 62000,
        "Pennsylvania": 37500,
        "Illinois": 38240,
        "Ohio": 35210,
        "Georgia": 31970,
        "North Carolina": 31160,
        "Michigan": 30000,
        "New Jersey": 38000,
        "Virginia": 25760,
        "Washington": 23000,
        "Arizona": 21340,
        "Tennessee": 20620,
        "Massachusetts": 20980,
        "Indiana": 20250,
        "Missouri": 20000,
        "Maryland": 20000,
        "Wisconsin": 17590,
        "Colorado": 17230,
        "Minnesota": 17030,
        "South Carolina": 15280,
        "Alabama": 14990,
        "Louisiana": 13900,
        "Kentucky": 13450,
        "Oregon": 12650,
        "Oklahoma": 11820,
        "Connecticut": 10760,
        "Utah": 9760,
        "Iowa": 9520,
        "Nevada": 9270,
        "Arkansas": 8990,
        "Mississippi": 8840,
        "Kansas": 8770,
        "New Mexico": 6320,
        "Nebraska": 5850,
        "Idaho": 5490,
        "West Virginia": 5350,
        "Hawaii": 4340,
        "New Hampshire": 4110,
        "Maine": 4070,
        "Montana": 3240,
        "Rhode Island": 3280,
        "Delaware": 2950,
        "South Dakota": 2650,
        "North Dakota": 2330,
        "Alaska": 2190,
        "Vermont": 1920,
        "Wyoming": 1720,
    },
)


class DiagnosisDistribution(ExplicitDistribution):
    def __init__(self, field_name: str):
        sum = 0
        frequency = []

        h = 0.0
        for j in range(1, 5001):
            h += 1 / (j**2)

        for i in range(1, 5001):
            f_i = math.ceil(1000000 * ((i**-2) / h))
            sum += f_i

        for i in range(1, 5001):
            f_i = math.ceil(1000000 * ((i**-2) / h))
            if i == 1 and sum > 1000000:
                f_i = f_i - (sum - 1000000)
            frequency.append(f_i)

        rnd = Random()
        rnd.seed(10000)
        diagnosis_code = {}
        for f in frequency:
            gen_fresh_code = lambda: "{}{:0>2}-{:0>2}".format(
                rnd.choice(string.ascii_uppercase), rnd.randrange(0, 99), rnd.randrange(0, 99)
            )
            fresh_code = gen_fresh_code()
            while fresh_code in diagnosis_code:
                fresh_code = gen_fresh_code()
            diagnosis_code[fresh_code] = f
        super(DiagnosisDistribution, self).__init__(field_name, 8, diagnosis_code)


class UniformDistribution(ExplicitDistribution):
    def __init__(self, field_name: str, contention_factor: int, values):
        numDocs = 1000000
        target = math.floor(numDocs / len(values))

        distribution = {}

        for value in values:
            distribution[value] = target

        super(UniformDistribution, self).__init__(field_name, contention_factor, distribution)


status_codes = UniformDistribution("status", 16, [f"A{x}" for x in range(1, 21)])


def make_credit_cards():
    faker = Faker()
    faker.seed_instance(12345)

    credit_cards = []
    for i in range(0, 100000):
        credit_cards.append(faker.unique.credit_card_number())

    return credit_cards


credit_cards = UniformDistribution("credit_cards", 4, make_credit_cards())


class SequenceDistribution(Distribution):
    def __init__(self, field_name: str, prefix: str):
        super(SequenceDistribution, self).__init__(field_name, 1)
        self.prefix = prefix

    def emit_generator(self):
        template = env.get_template("sequence_distribution.jinja2")
        return Snippet(template, {"field_name": self.field_name, "prefix": self.prefix})

    def emit_targetter(self):
        return f'{{^Join: {{array: [ "{self.prefix}", {{^FormatString: {{"format": "%|07d|", "withArgs": [{{^RandomInt: {{min: 0, max: 1000000}}}}]}}}}]}}}}'

    def emit_values(self):
        yield ()


guids = SequenceDistribution("guid", "99999999-9999-9999-99999")

distributions = [states, DiagnosisDistribution("diagnosis"), status_codes, credit_cards, guids]

for distribution in distributions:
    filename = f"{distribution.field_name}.txt"
    print(f"Writing {filename}")
    with open(filename, "w+") as dataFile:
        for value in distribution.emit_values():
            dataFile.write(f"{value}\n")

with open("maps_medical.yml", "w+") as mapFile:
    mapFile.write(template.render({"objFields": [x.emit_generator() for x in distributions]}))


class CreateIndexPhase:
    def __init__(self, env, field: str):
        self.env = env
        self.field = field

    def context(self):
        return {"field": self.field}

    def generate(self):
        template = self.env.get_template("create_index_phase.jinja2")
        return template


class LoadPhase:
    def __init__(self, env, numDocs):
        self.env = env
        self.numDocs = numDocs

    def context(self):
        return {"iterationsPerThread": math.floor(self.numDocs / 16)}

    def generate(self):
        template = self.env.get_template("load_phase.jinja2")
        return template


class FSMPhase:
    def __init__(self, env, distribution: Distribution, numOps: int, readUpdateRatio: (int, int)):
        self.env = env
        self.field_name = distribution.field_name
        self.targetter = distribution.emit_targetter()
        self.numOps = numOps
        self.readUpdateRatio = readUpdateRatio

    def context(self):
        return {
            "field_name": self.field_name,
            "targetter": self.targetter,
            "iterationsPerThread": math.floor(self.numOps / 16),
            "readRatio": self.readUpdateRatio[0] / 100,
            "updateRatio": self.readUpdateRatio[1] / 100,
        }

    def generate(self):
        template = self.env.get_template("update_query_mixed_phase.jinja2")
        return template


def to_snake_case(camel_case):
    """
    Converts CamelCase to snake_case, useful for generating test IDs
    https://stackoverflow.com/questions/1175208/
    :return: snake_case version of camel_case.
    """
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", camel_case)
    s2 = re.sub("-", "_", s1)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s2).lower()


testNames = []
for encrypted in [True, False]:
    for distribution in distributions:
        for ratio in [(100, 0), (95, 5), (50, 50)]:
            if encrypted:
                testName = f"medical_workload-{distribution.field_name}-{ratio[0]}-{ratio[1]}"
            else:
                testName = f"medical_workload-{distribution.field_name}-{ratio[0]}-{ratio[1]}-unencrypted"
            fileName = f"workload/{testName}.yml"
            testNames.append(to_snake_case(testName))

            print(f"Writing workload file: {fileName}")
            with open(fileName, "w+") as equalFile:
                workloadTemplate = env.get_template("medical_workload.jinja2")

                phases = [LoadPhase(env, 1000000), FSMPhase(env, distribution, 100000, ratio)]
                if not encrypted:
                    phases = [CreateIndexPhase(env, x.field_name) for x in distributions] + phases

                context = {
                    "encryptedFields": distributions,
                    "collectionName": "medical",
                    "threadCount": 16,
                    "phases": phases,
                    "maxPhase": len(phases) - 1,
                    "shouldAutoRun": True,
                }

                if not encrypted:
                    context.pop("encryptedFields")

                equalFile.write(workloadTemplate.render(context))

for encrypted in [True, False]:
    if encrypted:
        fileName = "workload/medical_workload-load.yml"
    else:
        fileName = "workload/medical_workload-load-unencrypted.yml"
    testNames.append(to_snake_case(testName))
    print(f"Writing workload file: {fileName}")
    with open(fileName, "w+") as equalFile:
        workloadTemplate = env.get_template("medical_workload.jinja2")

        phases = [LoadPhase(env, 1000000)]
        if not encrypted:
            phases = [CreateIndexPhase(env, x.field_name) for x in distributions] + phases

        context = {
            "encryptedFields": distributions,
            "collectionName": "medical",
            "threadCount": 16,
            "phases": phases,
            "maxPhase": len(phases) - 1,
            "shouldAutoRun": True,
        }
        if not encrypted:
            context.pop("encryptedFields")

        equalFile.write(workloadTemplate.render(context))

with open("patchConfig.yml", "w+") as patchConfig:
    template = env.get_template("patch_config.jinja2")

    patchConfig.write(template.render({"testNames": testNames}))
