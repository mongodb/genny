# Things I need:
"""
        self.project = runtime.get('project')
        self.version = runtime.get('version_id')
        self.variant = runtime.get('build_variant')
        self.task_name = runtime.get('task_name')
        self.task_id = runtime.get('task_id')
        self.execution_number = runtime.get('execution')
        self.mainline = not runtime['is_patch'] if 'is_patch' in runtime else False
        self.tests
        self.bucket
"""

from collections import namedtuple


CedarBucketConfig = namedtuple('CedarBucketConfig', [
    'api_key',
    'api_secret',
    'api_token',
    'region',
    'name',
    'prefix'
])


CedarTestArtifact = namedtuple('CedarTestArtifact', [
    'bucket',
    'path',
    'tags',  # [str]
    'local_path',

    # The following properties are booleans.
    'is_ftdc',
    'is_bson',
    'is_uncompressed'
    # TODO: fill in the rest
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

