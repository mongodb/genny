import subprocess

from curator import poplar_grpc


def main_canaries_runner():
    """
    Intended to be the main entry point for running canaries.
    """

    with poplar_grpc():
        res = subprocess.run("genny-canaries")
        res.check_returncode()


if __name__ == "__main__":
    main_canaries_runner()
