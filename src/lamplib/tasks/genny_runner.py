import subprocess

from curator import poplar_grpc

def main_genny_runner():
    """
    Intended to be the main entry point for running Genny.
    """
    with poplar_grpc():
        res = subprocess.run("genny_core")
        res.check_returncode()


if __name__ == "__main__":
    main_genny_runner()
