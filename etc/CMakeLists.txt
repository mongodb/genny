install(DIRECTORY workloads/
        DESTINATION etc/genny/workloads
        COMPONENT Workloads
        FILES_MATCHING
            PATTERN "*.yml"
            PATTERN "*.yaml"
)
