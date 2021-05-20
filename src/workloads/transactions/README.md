Long Lived Transactions
=====

Currently, genny does not support parameterization, but the LLT workloads are highly repetitive.

For the moment, the __LLT*LowLtc.yml__ files can be considered the source files for the other configurations.

To generate the other configurations, run the following commands from the root directory:

```bash
for file in src/workloads/transactions/LLT*LowLtc.yml; do
    sed -e 's/^  GlobalRateValue: .*$/  GlobalRateValue: \&GlobalRateValue 800 per 1 second/' \
         -e 's/^  ThreadsValue: .*$/  ThreadsValue: \&ThreadsValue 16/' ${file} > ${file/Low/High}
done
for file in src/workloads/transactions/LLT*Ltc.yml; do
    sed -e 's/^  InitialDocumentCount: .*$/  InitialDocumentCount: \&InitialNumDocs 49000000/' \
        -e 's/^  SecondaryDocumentCount: .*$/  SecondaryDocumentCount: \&SecondaryNumDocs 10000000/' \
        -e 's/^  # In-memory: Database size works out about 12GB.$/  # Not In-memory: Database size works out about 30GB./' \
        ${file} > ${file/Ltc/Gtc}
done
```
