#Add test runs of examples here. 

add_test(doAll mwg -l error ../examples/doAll.yml)
add_test(forN mwg -l error ../examples/forN.yml)
add_test(ifNode mwg -l error ../examples/ifNode.yml)
add_test(operations mwg -l error ../examples/operations.yml)
add_test(insertMany mwg -l error ../examples/insertMany.yml)
add_test(insertRandom mwg -l error ../examples/insertRandom.yml)
add_test(insertSequential mwg -l error ../examples/insertSequential.yml)
add_test(override mwg -l error ../examples/override.yml)
add_test(sample1 mwg -l error ../examples/sample1.yml)
add_test(sample1+threads mwg -l error ../examples/sample1+threads.yml)
add_test(sample1+threadsNoSleep mwg -l error ../examples/sample1+threadsNoSleep.yml)
add_test(sample2 mwg -l error ../examples/sample2.yml)
add_test(speed mwg -l error ../examples/speedtest.yml)
add_test(workloadNode mwg -l error ../examples/workloadNode.yml)
# Check for error messages or fatal
SET_TESTS_PROPERTIES(operations ifNode insertMany insertSequential override PROPERTIES FAIL_REGULAR_EXPRESSION "error;fatal")
SET_TESTS_PROPERTIES(doAll ifNode forN operations insertMany insertRandom
insertSequential override sample1 sample1+threads
sample1+threadsNoSleep sample2 speed workloadNode PROPERTIES WORKING_DIRECTORY  "${PROJECT_BINARY_DIR}")
