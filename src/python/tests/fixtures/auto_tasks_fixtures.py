# 3-Tuples of (workload_dict, env_dict, expected)
workload_should_autorun_cases = [
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux',
                    'storageEngine': 'wiredTiger'
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'true'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, True),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, True),
    ({
        'AutoRun': {
            'Requires': {
                'runtime': {},
                'bootstrap': {}
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, True),
    ({
        'AutoRun': {
            'Requires': {}
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, True),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': ['osx', 'windows', 'linux']
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'true'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, True),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': ['osx', 'windows', 'debian']
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'true'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': ['osx', 'windows', 'linux']
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'false'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux',
                    'storageEngine': 'wiredTiger'
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'true'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'osx',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux',
                    'storageEngine': 'wiredTiger'
                },
                'runtime': {
                    'build_variant': 'variant1',
                    'is_patch': 'true'
                },
                'other': {
                    'key': 'value'
                }
            }
        }
    }, {
        'bootstrap': {
            'platform': 'osx',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux',
                    'storageEngine': 'other'
                },
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({}, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {}
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': 'not-a-dict'
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
    ({
        'AutoRun': {
            'Requires': {
                'bootstrap': {
                    'platform': 'linux',
                    'storageEngine': 'wiredTiger'
                },
                'runtime': 'string-runtime'
            }
        }
    }, {
        'bootstrap': {
            'platform': 'linux',
            'storageEngine': 'wiredTiger'
        },
        'runtime': {
            'build_variant': 'variant1',
            'is_patch': 'true'
        }
    }, False),
]
