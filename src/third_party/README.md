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

## Static Linking vs Shared Linking

### Linking with Shared Objects

Shared linking means that when the genny executable is run, the system linker needs to find all of
the shared objects that the executable was linked against. These libraries can be seen by running
`lld genny` on linux or `otool -L genny` on macOS. If a library is installed in the system path,
for instance if installed via the package manager, the linker usually finds them trivially. Linking
with shared objects allows reuse of the the shared object code between executables. It also
potentially allows the user to provide their own version of the shared object tailored to their
machine.

### Linking with Static Archives

Static linking means that the necessary parts of the library are included in the genny executable.
This increases the size of the executable and can potentially cause parts of the library to be
duplicated in separate executables. However, since no additional file needs to be passed around,
statically linking libraries makes distribution of the end library or executable easier. There are a
few concerns around statically linking a library:
* If the statically linked library requires other dependency libraries, then any binary that links
  that library needs to be linked to either static or shared forms of those dependency libraries.
* If the statically linked library is later linked as a shared library, then there may be duplicate
  symbol errors or unexpected behavior.
* If the statically linked library has a restrictive license, then it is possible that any binary
  that links that library will be subject to that same license.
