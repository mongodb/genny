import sys
import subprocess

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

def main_genny_runner():

    poplar = subprocess.Popen(get_poplar_args())

    res = subprocess.run(get_genny_args())
    res.check_returncode()

    poplar.terminate()
    poplar.wait(timeout=5)

if __name__ == '__main__':
    main_genny_runner()
