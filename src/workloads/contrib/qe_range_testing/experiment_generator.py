from jinja2 import Environment, PackageLoader, select_autoescape
from data_generation import generate_all_data
import os.path

env = Environment(
    loader=PackageLoader("qe_range_testing"), autoescape=select_autoescape()
)

# Path to mongo crypt locally, point to your local mongo_crypt_v1.so object
MONGO_CRYPT_PATH = "/home/ubuntu/mongo_crypt/lib/mongo_crypt_v1.so"

# Experimental parameters
N_THREADS = 16
DOCUMENT_COUNT = 100000
QUERY_COUNT = 100000
assert DOCUMENT_COUNT % (N_THREADS) == 0
assert QUERY_COUNT % (N_THREADS) == 0

# Data file paths
RC_AGE_FILE = "data/rc_ages.txt"
RC_BALANCE_FILE = "data/rc_balances.txt"
RC_TIMESTAMP_FILE = "data/rc_timestamps.txt"

RC_AGE_UPD_FILE = "data/rc_ages_up.txt"
RC_BALANCE_UPD_FILE = "data/rc_balances_up.txt"
RC_TIMESTAMP_UPD_FILE = "data/rc_timestamps_up.txt"

# Experiment-related info
query_templates = {
    "age": ["t"],
    "timestamp": ["t1", "t2", "t3"],
    "balance": ["t1", "t2", "t3"],
}

cfs = {"age": [8], "timestamp": [4], "balance": [8]}

tfs = {"age": [6], "timestamp": [8], "balance": [6]}

default_cf_tfs = {
    "age_cf": 8,
    "timestamp_tf": 8,
    "balance_cf": 8,
    "balance_tf": 6,
}

fsm_params = [
    {"do_fsm": True, "query_ratio": 1},
    {"do_fsm": True, "query_ratio": 0.95},
    {"do_fsm": True, "query_ratio": 0.5},
    {"do_fsm": False},
]


def get_field_dict():
    return {
        "age": {
            "field_name": "age_hospitals",
            "field_type": "Int32",
            "insert_file": RC_AGE_FILE,
            "update_file": RC_AGE_UPD_FILE,
        },
        "timestamp": {
            "field_name": "tm_retail_tx",
            "field_type": "Int",
            "insert_file": RC_TIMESTAMP_FILE,
            "update_file": RC_TIMESTAMP_UPD_FILE,
        },
        "balance": {
            "field_name": "bnk_bal",
            "field_type": "Decimal",
            "insert_file": RC_BALANCE_FILE,
            "update_file": RC_BALANCE_UPD_FILE,
        },
    }


# Generate the experiment name based on the experiment object
def experiment_name(ex):
    print(ex)
    if ex["do_fsm"]:
        ratio = f'read{int(round(ex["query_ratio"]*100))}'
        ft = f"{ex['field']}_{ex['template']}_"
    else:
        ratio = "insert_only"
        ft = ""

    if ex["encrypt"]:
        cf_tf = "encrypted"
    else:
        cf_tf = "unencrypted"

    return f"qe_range_rc_{ft}{ratio}_{cf_tf}"


# Prepend file names with given basedir and add to experiment object
def add_basedir_info_to_experiment(basedir, experiment):
    if experiment["do_fsm"]:
        experiment["min_file"] = (
            basedir
            + f'queries/rc_{experiment["field"]}_{experiment["template"]}_min.txt'
        )
        experiment["max_file"] = (
            basedir
            + f'queries/rc_{experiment["field"]}_{experiment["template"]}_max.txt'
        )
        experiment["insert_file"] = basedir + experiment["insert_file"]
        experiment["update_file"] = basedir + experiment["update_file"]
    experiment["timestamp_file"] = basedir + RC_TIMESTAMP_FILE
    experiment["age_file"] = basedir + RC_AGE_FILE
    experiment["balance_file"] = basedir + RC_BALANCE_FILE


# Enumerate and return all the experiment objects to be run
def rc_experiments(basedir):
    experiments = []
    for fsm_param in fsm_params:
        if fsm_param["do_fsm"]:
            for field, field_dict in get_field_dict().items():
                for template in query_templates[field]:
                    base_dict = {"field": field, "template": template}
                    for encrypt in [True, False]:
                        if encrypt:
                            for cf in cfs[field]:
                                for tf in tfs[field]:
                                    experiment = {
                                        "encrypt": encrypt,
                                        "cf": cf,
                                        field + "_cf": cf,
                                        "tf": tf,
                                        field + "_tf": tf,
                                        **base_dict,
                                        **field_dict,
                                        **fsm_param,
                                    }
                                    add_basedir_info_to_experiment(basedir, experiment)
                                    experiments.append(experiment)
                        else:
                            experiment = {
                                "encrypt": encrypt,
                                **base_dict,
                                **field_dict,
                                **fsm_param,
                            }
                            add_basedir_info_to_experiment(basedir, experiment)
                            experiments.append(experiment)
        else:
            for encrypt in [True, False]:
                experiment = {"encrypt": encrypt, **fsm_param}
                add_basedir_info_to_experiment(basedir, experiment)
                experiments.append(experiment)

    return experiments


# Generate and write all workload files to be run
# If is_local, generate for local testing, otherwise generate for evergreen testing
def generate_rc_workloads(is_local):
    if is_local:
        basedir = "./src/workloads/contrib/qe_range_testing/"
        crypt_path = MONGO_CRYPT_PATH
        if not os.path.exists(crypt_path):
            print(
                "Please point MONGO_CRYPT_PATH to the mongo_crypt_v1.so shared library for local workload generation."
            )
            exit(1)
        wldir = "local"
    else:
        basedir = "./src/genny/src/workloads/contrib/qe_range_testing/"
        crypt_path = "/data/workdir/mongocrypt/lib/mongo_crypt_v1.so"
        wldir = "evergreen"
    template = env.get_template("rc.yml.j2")
    experiments = rc_experiments(basedir)
    for ex in experiments:
        name = experiment_name(ex)
        kwargs = {**default_cf_tfs, **ex}
        with open(f"workloads/{wldir}/{name}.yml", "w") as f:
            f.write(
                template.render(
                    document_count=DOCUMENT_COUNT,
                    insert_threads=N_THREADS,
                    query_count=QUERY_COUNT,
                    query_threads=N_THREADS,
                    use_crypt_shared_lib=True,
                    crypt_shared_lib_path=crypt_path,
                    **kwargs,
                )
            )


# Generate an experiment config file for use in perf_tooling
# Will generate with an empty patch ID to be filled in by user
def generate_rc_config_file():
    patch_id = "<patch-id>"
    experiments = [experiment_name(ex) for ex in rc_experiments(".")]
    template = env.get_template("rc-perfconfig.yml.j2")

    with open("generated/rc_perfconfig.yml", "w") as f:
        f.write(
            template.render(
                patch_id=patch_id, experiments=experiments, n_threads=N_THREADS
            )
        )


if __name__ == "__main__":
    generate_all_data("./", DOCUMENT_COUNT, QUERY_COUNT)
    generate_rc_workloads(is_local=False)
    generate_rc_workloads(is_local=True)
    generate_rc_config_file()
