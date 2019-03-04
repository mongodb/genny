import datetime
import json
import os
import sys
from collections import namedtuple

from genny import cedar

CedarBucketConfig = namedtuple('CedarBucketConfig', [
    'api_key',
    'api_secret',
    'api_token',  # TODO: what is this.
    'region',
    'name',
    'prefix'
])


CedarTestArtifact = namedtuple('CedarTestArtifact', [
    'bucket',
    'path',
    'tags',  # [str]
    'local_path',
    'created_at',
    'is_uncompressed',
])


CedarTestInfo = namedtuple('CedarTestInfo', [
    'test_name',
    'trial',
    'tags',  # [str]
    'args'  # {str: str}
])


CedarTest = namedtuple('CedarTest', [
    'info',  # CedarTestInfo
    'created_at',
    'completed_at',
    'artifacts',  # [CedarTestArtifact]
    'metrics',  # unused
    'sub_tests'  # unused
])


CedarReport = namedtuple('CedarReport', [
    'project',
    'version',
    'variant',
    'task_name',
    'task_id',
    'execution_number',
    'mainline',
    'tests',  # [CedarTest]
    'bucket'  # BucketConfig
])


class _Config(object):
    """
    OO representation of environment variables used by this file.
    """
    def __init__(self, env, metrics_file_names, test_run_time):
        # EVG related.
        self.project = env['EVG_project']
        self.version = env['EVG_version']
        self.variant = env['EVG_variant']
        self.task_name = env['EVG_task_name']
        self.task_id = env['EVG_task_id']
        self.execution_number = env['EVG_execution_number']
        self.mainline = (env['EVG_is_patch'] == 'false')

        # We set these for convenience.
        self.test_name = env['test_name']
        self.metrics_file_names = metrics_file_names
        self.test_run_time = test_run_time
        self.now = datetime.datetime.utcnow()

        # AWS related.
        self.api_key = env['aws_key']
        self.api_secret= env['aws_secret']
        self.cloud_region = 'us-east-1'  # N. Virginia.
        self.cloud_bucket = 'dsi-genny-metrics'

    @property
    def created_at(self):
        return self.now - self.test_run_time


def build_artifacts(config):
    artifacts = []

    for path in config.metrics_file_names:
        base_name = os.path.basename(path)
        a = CedarTestArtifact(
            bucket=config.cloud_bucket,
            path=base_name,
            tags=[],
            local_path=path,
            created_at=config.created_at,
            is_uncompressed=True
        )
        artifacts.append(a)

    return artifacts


def build_bucket_config(config):
    bucket_prefix = '{}_{}'.format(config.task_id, config.execution_number)

    return CedarBucketConfig(
        api_key=config.api_key,
        api_secret=config.api_secret,
        api_token=None,
        region=config.cloud_region,
        name=config.cloud_bucket,
        prefix=bucket_prefix
    )


def build_tests(config):
    test_info = CedarTestInfo(
        test_name=config.test_name,
        trial=0,
        tags=[],
        args={}
    )

    test = CedarTest(
        info=test_info,
        created_at=config.created_at,
        completed_at=config.now,
        artifacts=build_artifacts(config),
        metrics=None,
        sub_tests=None
    )

    return test


def build_report(config):
    report = CedarReport(
        project=config.project,
        version=config.version,
        variant=config.variant,
        task_name=config.task_name,
        task_id=config.task_id,
        execution_number=config.execution_number,
        mainline=config.mainline,
        tests=build_tests(config),
        bucket=build_bucket_config(config)
    )

    return report


def build_parser():
    parser = cedar.build_parser()
    parser.description += " and create a cedar report"
    parser.add_argument('report_file', metavar='report-file', help='path to generated report file')
    parser.add_argument('--test-name', help='human friendly name for this test, defaults to the '
                                            'EVG_task_name environment variable')
    return parser


def main__cedar_report(argv=sys.argv[1:]):
    parser = build_parser()
    args = parser.parse_args(argv)
    env = os.environ.copy()

    if args.test_name:
        env['test_name'] = args.test_name
    else:
        env['test_name'] = env['EVG_task_name']

    metrics_file_names, test_run_time = cedar.run(args)
    config = _Config(env, metrics_file_names, test_run_time)

    report = build_report(config)

    with open(args.report_file, 'w') as f:
        json.dump(report._asdict(), f)
