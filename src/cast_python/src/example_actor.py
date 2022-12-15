"""This is an example of how to use the python actor support. You can write a CLI using click
and expect the first argument passed will be a path to the workload yaml file. For example,
given the following actor config:

- Name: HelloWorld
  Type: Python
  Threads: 1
  Phases:
    - Repeat: 1
      Module: example_actor
      Endpoint: hello_world


genny would invoke the following command to run this actor:

python -m example_actor hello_world <path_to_workload> 2>&1


Note - support for python actors is very experimental. Please reach out
to the perf team if you have a use-case you would like to try this support for
"""
import click


@click.group(name="ExamplePythonActor", context_settings=dict(help_option_names=["-h", "--help"]))
def cli():
    pass


@cli.command("hello_world", help=("Prints Hello World"))
@click.argument("workload_yaml", nargs=1)
def hello_world(workload_yaml: str):
    print(f"Hello world from {workload_yaml}")


if __name__ == "__main__":
    cli()
