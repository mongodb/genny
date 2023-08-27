import os
import time
import requests
import functools
from concurrent.futures import ThreadPoolExecutor
import click
import yaml


def _get_connection_urls(workload_yaml):
    with open(workload_yaml) as f:
        workload = yaml.safe_load(f)
    uris = workload.get("EnvironmentDetails", {}).get("MongosyncConnectionURIs")
    if not uris:
        raise Exception(
            f"This actor requires setting EnvironmentDetails: MongosyncConnectionURIs to use this script"
        )
    return uris


def poll(workload_yaml, predicate, key):
    """
    Wait for all mongosyncs to reach a certain state (e.g. predicate returns False)
    based on a value returned by the /progress endpoint
    """
    connection_urls = _get_connection_urls(workload_yaml)

    def get_progress():
        res = requests.get(f"{url}/api/v1/progress")
        return res.json()["progress"][key]

    for url in connection_urls:
        info = get_progress()
        while predicate(info):
            time.sleep(1)
            print(f"Polling {url} for {key}, current value = {info}", flush=True)
            info = get_progress()


def _change_one_mongosync_state(route, body, url):
    """
    Change state of a given mongosync running at the provided url
    """
    resp = requests.post(f"{url}{route}", json=body)
    print(resp.json(), flush=True)
    success = resp.json()["success"]
    if not success:
        raise Exception(f"State change failed at route {route}")
    return success


def change_state(workload_yaml, route, body):
    """
    Helper function to change state of mongosync. This must
    send all requests in parallel, as some commands block until
    all instances recieve them
    """

    connection_urls = _get_connection_urls(workload_yaml)

    fn = functools.partial(_change_one_mongosync_state, route, body)
    with ThreadPoolExecutor() as executor:
        futures = []
        # Using executor.map swallows exceptions from the task,
        # using .submit and then accessing the future's .result
        # will cause exceptions to be rethrown
        for url in connection_urls:
            futures.append(executor.submit(fn, url))
        for f in futures:
            f.result()


@click.group(name="MongosyncActor", context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    pass


@cli.command(
    "start",
    help=("Issue /start to all mongosync processes"),
)
@click.argument("workload_yaml", nargs=1)
def start(workload_yaml):
    change_state(workload_yaml, "/api/v1/start", {"Source": "cluster0", "Destination": "cluster1"})


@cli.command(
    "poll_for_cea",
    help=("Poll all available instances for the CEA stage"),
)
@click.argument("workload_yaml", nargs=1)
def poll_for_cea(workload_yaml):
    poll(workload_yaml, lambda x: x != "change event application", "info")


@cli.command(
    "poll_for_commit_point",
    help=("Wait till all the instances canCommit = true and lagTimeSeconds < 120"),
)
@click.argument("workload_yaml", nargs=1)
def poll_for_commit_point(workload_yaml):
    poll(workload_yaml, lambda x: bool(x) == False, "canCommit") or poll(
        workload_yaml, lambda x: int(x) > 120, "lagTimeSeconds"
    )


@cli.command(
    "drain_writes",
    help=("Wait till all writes have been drained to the destination cluster"),
)
@click.argument("workload_yaml", nargs=1)
def drain_writes(workload_yaml):
    poll(workload_yaml, lambda x: int(x) > 5, "lagTimeSeconds")


@cli.command(
    "commit",
    help=("Commit the migration"),
)
@click.argument("workload_yaml", nargs=1)
def commit(workload_yaml):
    change_state(workload_yaml, "/api/v1/commit", {})


@cli.command(
    "wait_for_commit",
    help=("Wait until all mongosyncs are finished commiting the migration"),
)
@click.argument("workload_yaml", nargs=1)
def wait_for_commit(workload_yaml):
    poll(workload_yaml, lambda x: x != "COMMITTED", "state")

@ cli.command(
        "verify",
    help=("data consistency check"),
)
@click.argument("workload_yaml", nargs=1)
def commit(workload_yaml):
    change_state(workload_yaml, "/api/v1/verify", {})

@cli.command(
    "wait_for_verify_done",
    help=("Wait until verification is done"),
)
@click.argument("workload_yaml", nargs=1)
def wait_for_commit(workload_yaml):
    poll(workload_yaml, lambda x: x != "VERIFYDONE", "state")

@cli.command(
    "pause",
    help=("Pause the migration"),
)
@click.argument("workload_yaml", nargs=1)
def commit(workload_yaml):
    change_state(workload_yaml, "/api/v1/pause", {})


@cli.command(
    "resume",
    help=("Resume the migration"),
)
@click.argument("workload_yaml", nargs=1)
def commit(workload_yaml):
    change_state(workload_yaml, "/api/v1/resume", {})

cli.command(
   "pause",
    help=("Pause the migration"),
)
@click.argument("workload_yaml", nargs=1)
def pause(workload_yaml):
    change_state(workload_yaml, "/api/v1/pause", {})


@cli.command(
    "resume",
    help=("Resume the migration"),
)
@click.argument("workload_yaml", nargs=1)
def resume(workload_yaml):
    change_state(workload_yaml, "/api/v1/resume", {})


if __name__ == "__main__":
    cli()
