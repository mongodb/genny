import os
import time
import requests
import functools
from concurrent.futures import ThreadPoolExecutor


def _get_connection_urls():
    mongosync_connection_var = "MongosyncConnectionUrls"
    connection_urls = os.environ.get(mongosync_connection_var)
    if not connection_urls:
        raise Exception(
            f'The environment variable "{mongosync_connection_var}" must be set to use this script'
        )
    return connection_urls.split()


def poll(predicate, key):
    """
    Wait for all mongosyncs to reach a certain state (e.g. predicate returns False)
    based on a value returned by the /progress endpoint
    """
    connection_urls = _get_connection_urls()

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


def change_state(route, body):
    """
    Helper function to change state of mongosync. This must
    send all requests in parallel, as some commands block until
    all instances recieve them
    """

    connection_urls = _get_connection_urls()

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
