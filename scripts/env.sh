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
    if [[ ! -e "$LAMP_VENV_DIR/venv/.setup-complete" ]]; then
        echo "Installing virtual environment."
        echo "To run without a virtual environment, use the -g or --run-global option."
        echo "creating new env with python: $(command -v python3)"
        pushd "$LAMP_VENV_DIR" >/dev/null
            python3 -m venv venv
        popd >/dev/null
    fi

    set +u
        source "${LAMP_VENV_DIR}/venv/bin/activate"
    set -u

    if [[ ! -e "$LAMP_VENV_DIR/venv/.setup-complete" ]]; then
        python3 -m pip install -e "$LAMP_VENV_DIR/../src/python"
        touch "$LAMP_VENV_DIR/venv/.setup-complete"
    fi
fi
