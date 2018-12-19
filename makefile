# specify a list of test binary names to support dependencies for
# running tests from the makefile.
test_binaries := test_driver test_gennylib


# start flag config
#   This should handle different operating system
#   requirements. Perhaps the operating system discoery should be
#   generated and not arch-centric. Most of your edits will be here.
cxx_flags := -pthread
FLAGS := -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ifeq (,$(shell ls /etc/arch-release))
  FLAGS += -DCMAKE_CXX_COMPILER=clang++-6.0
else
  FLAGS += -DCMAKE_CXX_COMPILER=gcc 8.2.1
  cxx_flags += -lstdc++
  cxx_flags += -lm
endif
FLAGS += -DCMAKE_CXX_FLAGS="$(cxx_flags)"
# end flag config


###################################


# start buildsystem setup
#   this handles running cmake and making sure that we rerun cmake
#   when things change.
build/.setup:makefile CMakeLists.txt
	mkdir -p build
	cd build; cmake $(FLAGS) ..
	touch $@
clean:
	rm -rf build
.PHONY:clean build
# end buildsystem setup.


# start cmake delegates
#   this is is jsut
build:genny
genny:
%:
	@$(MAKE) build/.setup
	cd build; $(MAKE) $@
# end cmake delegates


# start: set up test execution and building.
#   These are to support development workflows.
test:$(test_binaries)
	cd build; $(MAKE) $@
test-verbose:$(test_binaries)
	cd build; bash -c 'for i in `find . -type f -perm /111 -print | grep "test_"`; do $$i; done'
test-%:test_%
	cd build; bash -c 'for i in `find . -type f -perm /111 -print | grep "$<"`; do $$i; done'
# end testing
