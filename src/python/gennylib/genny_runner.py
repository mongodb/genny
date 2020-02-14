import sys
import subprocess
from contextlib import contextmanager

def get_genny_args():
    """
    Returns the argument list used to create the Genny process.
    """
    args = sys.argv
    args[0] = "genny_core"
    return args

def get_poplar_args():
    """
    Returns the argument list used to create the Poplar gRPC process.
    """
    return ['curator', 'poplar', 'grpc']

@contextmanager
def poplar_grpc():
    poplar = subprocess.Popen(get_poplar_args())
    try:
        yield poplar
    finally:
        poplar.terminate()
        if (poplar.wait(timeout=5) != 0):
            raise OSError("Poplar failed to terminate correctly.")

def main_genny_runner():
    """
    Intended to be the main entry point for running Genny.
    """
    
    with poplar_grpc():
        res = subprocess.run(get_genny_args())
        res.check_returncode()
    
if __name__ == '__main__':
    main_genny_runner()
