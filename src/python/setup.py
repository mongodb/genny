from setuptools import setup

setup(name='genny',
      version='1.0',
      packages=['genny'],
      test_suite='tests',
      entry_points={
          'console_scripts': [
              'genny-summarize-metrics = genny.metrics_output_parser:summarize'
          ]
      },
)