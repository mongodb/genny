#!/bin/bash

echo "assuming data dir = $1 and repeat count = $2"
echo ""
files=`ls $1/*Lookup*Field.ftdc`
echo $files
for f in $files
do
  echo processing $f...
  vals=`./run-genny export $f | tail -n $2 | cut -d , -f 7`
  prev=0
  for v in $vals
  do
    diff=`expr $v - $prev`
    prev=$v
    echo $diff
  done
done
