import sys
import os
import subprocess
from contextlib import contextmanager

def get_genny_args():
    """
    Returns the argument list used to create the Genny process.

    If we are in the root of the genny repo, use the local executable. 
    Otherwise we search the PATH.
    """
    args = sys.argv
    genny_core = "genny_core"

    local_core = "dist/bin/genny_core"
    if os.path.exists(local_core):
        genny_core = local_core
    args[0] = genny_core
    return args

def get_poplar_args():
    """
    Returns the argument list used to create the Poplar gRPC process.
    """
    return ['/data/mci/curator/curator', 'poplar', 'grpc']

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
