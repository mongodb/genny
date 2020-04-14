import datetime
import logging
import json
import os
import subprocess
import sys
import yaml

from collections import namedtuple

import requests

from gennylib.parsers import cedar

CedarBucketConfig = namedtuple(
    "CedarBucketConfig", ["api_key", "api_secret", "api_token", "region", "name"]
)

CedarTestArtifact = namedtuple(
    "CedarTestArtifact",
    [
        "bucket",
        "path",
        "tags",  # [str]
        "local_path",
        "created_at",
        "convert_bson_to_ftdc",
        "prefix",
        "permissions",
    ],
)

CedarTestInfo = namedtuple(
    "CedarTestInfo", ["test_name", "trial", "tags", "args"]  # [str]  # {str: str}
)

CedarTest = namedtuple(
    "CedarTest",
    [
        "info",  # CedarTestInfo
        "created_at",
        "completed_at",
        "artifacts",  # [CedarTestArtifact]
        "metrics",  # unused
        "sub_tests",  # unused
    ],
)

CedarReport = namedtuple(
    "CedarReport",
    [
        "project",
        "version",
        "order",
        "variant",
        "task_name",
        "task_id",
        "execution_number",
        "mainline",
        "tests",  # [CedarTest]
        "bucket",  # BucketConfig
    ],
)

DEFAULT_REPORT_FILE = "cedar_report.json"
DEFAULT_DATE_FORMAT = "%Y-%m-%dT%H:%M:%SZ"


class _Config(object):
    """
    OO representation of environment variables used by this file.
    """

    def __init__(self, env, metrics_file_names, test_run_time):
        # EVG related.
        self.project = env["project"]
        self.version = env["version_id"]
        self.variant = env["build_variant"]
        self.task_name = env["task_name"]
        self.task_id = env["task_id"]
        self.execution_number = int(env["execution"])
        # This env var is either the string "true" or unset.
        self.mainline = not (env.get("is_patch", "") == "true")

        try:
            self.order = int(env["revision_order_id"])
        except (ValueError, TypeError):
            self.order = None

        # We set these for convenience.
        self.test_name = env["test_name"]
        self.metrics_file_names = metrics_file_names
        self.test_run_time = test_run_time
        self.now = datetime.datetime.utcnow()

        # AWS related.
        self.api_key = env["terraform_key"]
        self.api_secret = env["terraform_secret"]
        self.cloud_region = "us-east-1"  # N. Virginia.
        self.cloud_bucket = "genny-metrics"

    @property
    def created_at(self):
        return self.now - self.test_run_time


def build_report(config, args):
    sub_tests = []

    bucket_prefix = "{}_{}".format(config.task_id, config.execution_number)

    for path in config.metrics_file_names:
        base_name = os.path.basename(path)
        test_name = os.path.splitext(base_name)[0]

        a = CedarTestArtifact(
            bucket=config.cloud_bucket,
            path=test_name,
            tags=[],
            local_path=path,
            created_at=config.created_at,
            convert_bson_to_ftdc=not cedar.is_ftdc(args),
            permissions="public-read",
            prefix=bucket_prefix,
        )

        ti = CedarTestInfo(test_name=test_name, trial=0, tags=[], args={})

        t = CedarTest(
            info=ti._asdict(),
            created_at=config.created_at,
            completed_at=config.now,
            artifacts=[a._asdict()],
            metrics=None,
            sub_tests=None,
        )
        sub_tests.append(t._asdict())

    bucket_config = CedarBucketConfig(
        api_key=config.api_key,
        api_secret=config.api_secret,
        api_token=None,
        region=config.cloud_region,
        name=config.cloud_bucket,
    )

    test_info = CedarTestInfo(test_name=config.test_name, trial=0, tags=[], args={})

    test = CedarTest(
        info=test_info._asdict(),
        created_at=config.created_at,
        completed_at=config.now,
        artifacts=[],
        metrics=None,
        sub_tests=sub_tests,
    )

    report = CedarReport(
        project=config.project,
        version=config.version,
        order=config.order,
        variant=config.variant,
        task_name=config.task_name,
        task_id=config.task_id,
        execution_number=config.execution_number,
        mainline=config.mainline,
        tests=[test._asdict()],
        bucket=bucket_config._asdict(),
    )

    return report._asdict()


class CertRetriever(object):
    """Retrieves client certificate and key from the cedar API using Jira username and password."""

    def __init__(self, username, password):
        self.auth = json.dumps({"username": username, "password": password})

    @staticmethod
    def _fetch(url, output, **kwargs):
        if os.path.exists(output):
            return output
        resp = requests.get(url, **kwargs)
        resp.raise_for_status()
        with open(output, "w") as pem:
            pem.write(resp.text)
        return output

    def root_ca(self):
        """
        :return: the root cert authority pem file from cedar
        """
        return self._fetch("https://cedar.mongodb.com/rest/v1/admin/ca", "cedar.ca.pem")

    def user_cert(self):
        """
        :return: the user-level pem
        """
        return self._fetch(
            "https://cedar.mongodb.com/rest/v1/admin/users/certificate",
            "cedar.user.crt",
            data=self.auth,
        )

    def user_key(self):
        """
        :return: the user-level key
        """
        return self._fetch(
            "https://cedar.mongodb.com/rest/v1/admin/users/certificate/key",
            "cedar.user.key",
            data=self.auth,
        )


def build_parser():
    parser = cedar.build_parser()
    parser.description += " and create a cedar report"
    parser.add_argument(
        "--report-file", default=DEFAULT_REPORT_FILE, help="path to generated report file"
    )
    parser.add_argument(
        "--test-name",
        help="human friendly name for this test, defaults to the "
        "EVG_task_name environment variable",
    )
    parser.add_argument(
        "--expansions-file",
        default="expansions.yml",
        help="expansions-file with configuration needed by cedar",
    )
    return parser


class RFCDateTimeEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, datetime.datetime):
            # RFC3339 format.
            return o.strftime(DEFAULT_DATE_FORMAT)
        return super().default(self, o)


def main__cedar_report(argv=sys.argv[1:], env=None, cert_retriever_cls=CertRetriever):
    """
    Generate a cedar report and upload it using poplar

    :param argv: command line argument
    :param env: shell environment; defaults to os.environ
    :param cert_retriever_cls: class for cert retriever, can be overridden if no certificates are required.
    :return:
    """
    logging.basicConfig(level=logging.INFO)
    parser = build_parser()
    args = parser.parse_args(argv)

    if not env:
        with open(args.expansions_file, "r") as f:
            env = yaml.safe_load(f)

    if args.test_name:
        env["test_name"] = args.test_name
    else:
        env["test_name"] = env["task_name"]

    metrics_file_names, test_run_time = cedar.run(args)
    config = _Config(env, metrics_file_names, test_run_time)

    report_dict = build_report(config, args)

    with open(args.report_file, "w") as f:
        json.dump(report_dict, f, cls=RFCDateTimeEncoder)


if __name__ == "__main__":
    main__cedar_report()
