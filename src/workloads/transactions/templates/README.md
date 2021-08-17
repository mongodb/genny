Long Lived Transactions
=====

Currently, genny does not support parameterization, but the LLT workloads are highly repetitive.

For the moment, the __LLT*LowLtc.tmpl__ files can be considered the source files for the other configurations.

To generate the other configurations, run the following commands from the root directory:

```bash
out_dir=src/workloads/transactions/templates/out
mkdir -pv $out_dir
for file in src/workloads/transactions/templates/LLT*LowLtc.tmpl; do
    cp $file $out_dir/$(basename ${file/tmpl/yml})
done
for file in $out_dir/LLT*LowLtc.yml; do
    sed -e 's/^  GlobalRateValue: .*$/  GlobalRateValue: \&GlobalRateValue 1 per 1250 microsecond/' \
         -e 's/^  ThreadsValue: .*$/  ThreadsValue: \&ThreadsValue 16/' ${file} > ${file/Low/High}
done
for file in $out_dir/LLT*Ltc.yml; do
    sed -e 's/^  InitialDocumentCount: .*$/  InitialDocumentCount: \&InitialNumDocs 49000000/' \
        -e 's/^  SecondaryDocumentCount: .*$/  SecondaryDocumentCount: \&SecondaryNumDocs 10000000/' \
        -e 's/^  # In-memory: Database size works out about 12GB.$/  # Not In-memory: Database size works out about 30GB./' \
        ${file} > ${file/Ltc/Gtc}
done
```

Copy them to the desired location, but for the moment we are only deploying the equivalent of the Mixed High and Low Ltc.