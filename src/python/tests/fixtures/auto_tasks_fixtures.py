# 3-Tuples of (workload_dict, env_dict, expected)
workload_should_autorun_cases = [
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["linux"],
                    "storageEngine": ["wiredTiger"],
                    "build_variant": ["variant1"],
                    "is_patch": ["true"],
                }
            }
        },
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        True,
    ),
    (
        {"AutoRun": {"Requires": {"platform": ["linux"]}}},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        True,
    ),
    (
        {"AutoRun": {"Requires": {"platform": ["linux"]}}},
        {
            "expansions": {
                "platform": "linux",
                "storageEngine": "wiredTiger",
                "build_variant": "variant1",
                "is_patch": "true",
            }
        },
        True,
    ),
    (
        {"AutoRun": {"Requires": {}}},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        True,
    ),
    (
        {"AutoRun": {"Requires": {}}},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        True,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["osx", "windows", "linux"],
                    "build_variant": ["variant1"],
                    "is_patch": ["true"],
                }
            }
        },
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        True,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["osx", "windows", "debian"],
                    "build_variant": ["variant1"],
                    "is_patch": ["true"],
                }
            }
        },
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["osx", "windows", "linux"],
                    "build_variant": ["variant1"],
                    "is_patch": ["false"],
                }
            }
        },
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["linux"],
                    "storageEngine": ["wiredTiger"],
                    "build_variant": ["variant1"],
                    "is_patch": ["true"],
                }
            }
        },
        {
            "bootstrap": {"platform": "osx", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["linux"],
                    "storageEngine": ["wiredTiger"],
                    "build_variant": ["variant1"],
                    "is_patch": ["true"],
                    "key": ["value"],
                }
            }
        },
        {
            "bootstrap": {"platform": "osx", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {"AutoRun": {"Requires": {"platform": ["linux"], "storageEngine": ["other"]}}},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {},
        {
            "bootstrap": {"platform": ["linux"], "storageEngine": ["wiredTiger"]},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {"AutoRun": {}},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {"AutoRun": "not-a-dict"},
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
    (
        {
            "AutoRun": {
                "Requires": {
                    "platform": ["linux"],
                    "storageEngine": ["wiredTiger"],
                    "runtime": ["string-runtime"],
                }
            }
        },
        {
            "bootstrap": {"platform": "linux", "storageEngine": "wiredTiger"},
            "runtime": {"build_variant": "variant1", "is_patch": "true"},
        },
        False,
    ),
]
