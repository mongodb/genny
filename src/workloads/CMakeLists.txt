# Install all the workload documents
install(DIRECTORY .
        DESTINATION etc/genny/workloads
        COMPONENT Workloads
        FILES_MATCHING
            PATTERN "*.yaml"
)