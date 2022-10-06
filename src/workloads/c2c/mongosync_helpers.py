import argparse
import os
import time
import requests
import functools
from concurrent.futures import ThreadPoolExecutor


def poll_mongosync(connection_urls, predicate, key):
    """
    Wait for all mongosyncs to reach a certain state (e.g. predicate returns False)
    based on a value returned by the /progress endpoint
    """

    def get_progress():
        res = requests.get(f"{url}/api/v1/progress")
        return res.json()["progress"][key]

    for url in connection_urls:
        info = get_progress()
        while predicate(info):
            time.sleep(1)
            print(f"Polling {url} for {key}, current value = {info}", flush=True)
            info = get_progress()


def change_one_mongosync_state(route, body, url):
    """
    Change state of a given mongosync running at the provided url
    """
    resp = requests.post(
        f"{url}{route}", json=body
    )
    print(resp.json(), flush=True)
    success = resp.json()["success"]
    if not success:
        raise Exception(f"State change failed at route {route}")
    return success


def change_mongosync_state(connection_urls, route, body):
    """
    Helper function to change state of mongosync. This must
    send all requests in parallel, as some commands block until
    all instances recieve them
    """
    fn = functools.partial(change_one_mongosync_state, route, body)
    with ThreadPoolExecutor() as executor:
        futures = []
        # Using executor.map swallows exceptions from the task,
        # using .submit and then accessing the future's .result
        # will cause exceptions to be rethrown
        for url in connection_urls:
            futures.append(executor.submit(fn, url))
        for f in futures:
            f.result()

def start_mongosync(connection_urls):
    """
    Start all mongosyncs
    """
    change_mongosync_state(connection_urls,
        "/api/v1/start",
        {"Source": "cluster0", "Destination": "cluster1"})

def poll_for_cea(connection_urls):
    """
    Poll until mongosync reaches the CEA state
    """
    poll_mongosync(connection_urls, lambda x: x != "change event application", "info")


def drain_writes(connection_urls):
    """
    Wait for mongosync to drain all writes, continuing to poll
    while lag is greater than 5s
    """
    poll_mongosync(connection_urls, lambda x: int(x) > 5, "lagTimeSeconds")


def wait_for_commit(connection_urls):
    """
    Wait until all mongosyncs have reached the COMMITED state
    """
    poll_mongosync(connection_urls, lambda x: x != "COMMITTED", "state")


def commit(connection_urls):
    """
    Commit the migration for all mongosyncs
    """
    change_mongosync_state(connection_urls, "/api/v1/commit", {})


def main (args):
    lookup = {
        "start": start_mongosync,
        "poll_for_cea": poll_for_cea,
        "drain_writes": drain_writes,
        "wait_for_commit": wait_for_commit,
        "commit": commit,
    }

    if args.mode in lookup:
        mongosync_connection_var = "MongosyncConnectionUrls"
        connection_urls = os.environ.get(mongosync_connection_var)
        if not connection_urls:
            raise Exception(f"The environment variable \"{mongosync_connection_var}\" must be set to use this script")
        connection_urls = connection_urls.split(" ")
        lookup[args.mode](connection_urls)
    else:
        raise Exception(f"Unknown mode {args.mode}")

parser = argparse.ArgumentParser()
parser.add_argument("-m", "--mode", help="Which mongosync helper to run")
args = parser.parse_args()
main(args)
