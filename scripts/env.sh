RUN_GLOBAL=0

for var in "$@"
do
  case "$var" in
    -g|--run-global)
      RUN_GLOBAL=1
      ;;
  esac
done

# for mongodbtoolchain python3
export PATH=/opt/mongodbtoolchain/v3/bin:$PATH

# Check if not called with -g or --run-global flag
if [[ $RUN_GLOBAL != 1 ]]; then
    if [[ ! -e "$SCRIPTS_DIR/venv/.setup-complete" ]]; then
        echo "Installing virtual environment."
        echo "To run without a virtual environment, use the -g or --run-global option."
        echo "creating new env with python: $(command -v python3)"
        pushd "$SCRIPTS_DIR" >/dev/null
            python3 -m venv venv
        popd >/dev/null
    fi

    set +u
        source "${SCRIPTS_DIR}/venv/bin/activate"
    set -u

    if [[ ! -e "$SCRIPTS_DIR/venv/.setup-complete" ]]; then
        python3 -m pip install -e "$SCRIPTS_DIR/../src/python"
        touch "$SCRIPTS_DIR/venv/.setup-complete"
    fi
fi
