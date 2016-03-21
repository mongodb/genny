#Add test runs of examples here.

add_test(doAll mwg -l error ../examples/doAll.yml)
add_test(forN mwg -l error ../examples/forN.yml)
add_test(ifNode mwg -l error ../examples/ifNode.yml)
add_test(operations mwg -l error ../examples/operations.yml)
add_test(insertMany mwg -l error ../examples/insertMany.yml)
add_test(insertRandom mwg -l error ../examples/insertRandom.yml)
add_test(insertSequential mwg -l error ../examples/insertSequential.yml)
add_test(override mwg -l error ../examples/override.yml)
add_test(sample1 mwg -l error ../examples/sample1.yml --variable
delay=100 --variable runLengthMs=1000)
add_test(sample1+threads mwg -l error ../examples/sample1+threads.yml --variable
delay=100 --variable runLengthMs=1000)
add_test(sample1+threadsNoSleep mwg -l error
../examples/sample1+threadsNoSleep.yml --runLength=1000)
add_test(sample2 mwg -l error ../examples/sample2.yml --variable delay=10)
add_test(speed mwg -l error ../examples/speedtest.yml)
add_test(workloadNode mwg -l error ../examples/workloadNode.yml)
add_test(spawn mwg -l error ../examples/spawn.yml)
add_test(sample+yamloverride mwg -l info ../examples/sample1.yml --variable runLengthMs=2000)
add_test(sample+cmdlinerunLength mwg -l info ../examples/sample1.yml --runLengthMs=2000)
add_test(includes mwg -l error ../examples/includes2.yml)
add_test(load_file mwg -l error ../examples/load_file.yml)

# With the override it should run the sleep twice.
SET_TESTS_PROPERTIES(sample+yamloverride sample+cmdlinerunLength
PROPERTIES PASS_REGULAR_EXPRESSION "sleep, Count=2") 

# Check for error messages or fatal
SET_TESTS_PROPERTIES(operations ifNode insertMany insertSequential
override spawn includes load_file PROPERTIES FAIL_REGULAR_EXPRESSION
"error;fatal")

SET_TESTS_PROPERTIES(doAll ifNode forN operations insertMany
insertRandom insertSequential override sample1 sample1+threads
sample1+threadsNoSleep sample2 speed workloadNode spawn
sample+yamloverride sample+cmdlinerunLength includes load_file
PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
