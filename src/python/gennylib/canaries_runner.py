import subprocess

from gennylib.genny_runner import poplar_grpc
from gennylib.genny_runner import get_program_args

def main_canaries_runner():
    """
    Intended to be the main entry point for running canaries.
    """

    with poplar_grpc():
        res = subprocess.run(get_program_args("genny-canaries"))
        res.check_returncode()

if __name__ == "__main__":
    main_canaries_runner()
