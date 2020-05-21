
DIR=$(pwd)

# for mongodbtoolchain python3
export PATH=/opt/mongodbtoolchain/v3/bin:$PATH

# Check if not called with -g or --run-global flag
if [[ "$*" != *-g* ]]; then
    if [[ ! -e "$SCRIPTS_DIR/venv/.setup-complete" ]]; then
        echo "Installing virtual environment."
        echo "To run without a virtual environment, use the -g or --run-global option."
        echo "creating new env with python: $(command -v python3)"
        pushd "$SCRIPTS_DIR" >/dev/null
            python3 -m venv venv
        popd >/dev/null
        touch "$SCRIPTS_DIR/venv/.setup-complete"
        pushd "$SCRIPTS_DIR/../src/python" >/dev/null
            python3 setup.py --quiet develop
        popd >/dev/null
    fi

    set +u
        source "${SCRIPTS_DIR}/venv/bin/activate"
    set -u
fi
