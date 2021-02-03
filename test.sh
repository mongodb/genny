TRIALS=1

./scripts/lamp

counter=0
status=0
while [ $counter -lt $TRIALS ] && [ $status -eq 0 ]
do
    echo Executing test.

    pushd ../mongo
    export PATH=$PWD:$PATH
    mlaunch stop
    sleep 2
    rm -rf data
    sleep 2
    #mlaunch init --single
    mlaunch init --replicaset
    #mlaunch init --replicaset --shards=2
    sleep 20
    popd

    rm build/genny-metrics/*; 
    ./scripts/genny run -w src/workloads/scale/InsertRemove.yml
    status=$?
    ((counter++))
done

wait

echo "--------------------------"
[ $status -eq 0 ] && echo "genny was successful" || echo "genny failed"
echo "--------------------------"

exit $status
