#!/bin/bash

echo "assuming repeat count = $1"
echo ""
files=`ls build/WorkloadOutput/CedarMetrics/*Lookup.Local_*Foreign_*.ftdc`
for f in $files
do
  echo processing $f...
  vals=`./run-genny export $f | tail -n $1 | cut -d , -f 7`
  prev=0
  for v in $vals
  do
    diff=`expr $v - $prev`
    prev=$v
    echo $diff
  done
done
