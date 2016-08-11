Dependencies
============

* The
  [C++11 mongo driver](https://github.com/mongodb/mongo-cxx-driver/tree/master). Currently
  tested against r3.0.1. If compiling on OS X, see
  [CXX-836](https://jira.mongodb.org/browse/CXX-836). The C++11 driver
  in turn requires the c driver.
* [yaml-cpp](https://github.com/jbeder/yaml-cpp)
* A compiler that supports c++11
* [cmake](http://www.cmake.org/) for building
* [Boost](http://www.boost.org/) for logging and filesystem
  libraries. Version 1.48 or later

Install Script
==============

Jonathan Abrahams has added an [install script](install.sh) that
should install all the dependencies and build the tool for CentOS,
Ubuntu, Darwin, and Windows_NT.

Building
========

    cd build
    cmake ..
    make
    make test # optional. Expects a mongod running on port 27017


Build Notes:

* The build will use static boost by default, and static mongo c++
  driver libraries if they exist. If boost static libraries don't
  exist on your system, add "-DBoost\_NON\_STATIC=true" to the end of your
  cmake line.
* On some systems I've had trouble linking against the mongo c
  driver. In those cases, editing libmongo-c-1.0.pc (usually in
  /usr/local/lib/pkgconfig/) can fix this. On the Libs: line, move the
  non mongoc libraries after "-lmongoc-1.0".
