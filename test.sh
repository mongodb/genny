TRIALS=1000

./run-genny install

counter=0
status=0
while [ $counter -lt $TRIALS ] && [ $status -eq 0 ]
do
    echo Executing test.
    ./run-genny dry-run-workloads -w src/workloads/scale/LargeIndexedIns.yml
    status=$?
    ((counter++))
done

wait

echo "--------------------------"
[ $status -eq 0 ] && echo "genny was successful" || echo "genny failed"
echo "--------------------------"

exit $status
