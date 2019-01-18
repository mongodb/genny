# Third Party Libraries

Genny depends on a set of third party libraries to compile and run.
These libraries are included in one of three general ways:
1. Toolchain libraries via find_package
2. Vendored libraries via add_subdirectory
3. User libraries via find_package

## Toolchain Libaries

Toolchain libraries are generally monolithic pieces of code with a large collection of headers.
If they can be linked via a static library, they should be. Generally speaking, these libraries are
expected to have a version that is acceptable for genny for the foreseeable future. Often, these
libraries are so essential that the CMake find_package module searches for their core headers at
known locations. For alternative locations, usually the module will allow the user to specify a
prefix dir. While distributing a specific version of these libraries may be necessary, there will
almost always be a known recipe for building them for a given distro.

Genny currently uses:
* libstdc++ (with c++ 17)
* libicuuc
* Boost (with log headers)
* OpenSSL 1.1

## Vendored Libraries

Vendored libraries are usually small libraries that facilitate a unique capability. 

Libraries should be vendored in when they fit the following criteria:
* They have no additional dependencies beyond what genny needs via its system libraries.
* They are expected to change version rarely. 
* They can be statically linked or compiled in as object files.

Genny currently uses:
* yaml-cpp
* Loki
* Catch2
* Jasper

## User Libraries

User libraries are modular pieces with a known ABI and some dependencies. Usually, linking to
a single shared library with a specific minor version of these libraries is sufficient.
These libraries will almost always provide either a pkg-config or CMake find_package config.
They may not have well defined recipes to build them on some distros.

Genny currently uses:
* gRPC
* libmongocxx
