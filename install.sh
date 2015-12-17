#!/bin/bash

# Set the variable $os
# Should work for CentOS, Ubuntu, Darwin (OSX) & Windows_NT
# os will be set only for Linux platforms
os=$($(uname -a | grep -q Linux) && echo $(grep ^NAME= /etc/os-release | cut -f2 -d \" | cut -f1 -d " "))

if [[ ! -z $OS && -z $os ]];then
    # Cygwin sets $OS
    os=$OS
elif [ -z $os ]; then
    # Darwin
    os=$(uname -s)
fi

case "$os" in
    Ubuntu)
        echo "Platform is $os"
        pkgmgr_install="sudo apt-get install -y"
#        pkgs="libtool openssl autoconf libboost-all-dev libyaml-cpp-dev"
        ;;
    CentOS)
        echo "Platform is $os"
        pkgmgr_install="sudo yum install -y"
        pkgs="libtool openssl autoconf"
        ;;
    Darwin)
        echo "Platform is $os"
        if [ -z $(which brew) ]; then
            echo "Install requires Homebrew, see http://brew.sh/"
            exit 1
        fi
        brew update
        pkgmgr_install="brew install"
        pkgs="openssl"
        c_flags="LDFLAGS='-L/usr/local/opt/openssl/lib' CPPFLAGS='-I/usr/local/opt/openssl/include'"
        cmake_install="brew upgrade cmake"
        boost_install="brew install boost"
        yaml_cpp_install="brew install yaml-cpp"
        ;;
    *)
        echo "Unsupported platform $os"
        exit 1
        ;;
esac

workgenDir=$(pwd)

# Install required packages
echo "Install packages: $pkgmgr_install $pkgs"
$pkgmgr_install $pkgs

# cmake install
echo "Install cmake"
if [ -z "$cmake_install" ]; then
    CMAKE=cmake-3.1.0-Linux-x86_64
    wget -qO- https://cmake.org/files/v3.1/${CMAKE}.tar.gz | tar -xzv
    sudo cp ${CMAKE}/bin/* /usr/local/bin/
    sudo cp -r ${CMAKE}/share/* /usr/local/share/
    sudo rm -fr $CMAKE
else
    $($cmake_install)
fi

# boost install (need boost 1.59)
echo "Install boost"
if [ -z "$boost_install" ]; then
    BOOST=boost_1_59_0
    wget -qO- http://sourceforge.net/projects/boost/files/boost/1.59.0/${BOOST}.tar.gz | tar -xzv
    pushd $BOOST
    sudo ./bootstrap.sh --with-libraries=log,regex
    sudo ./b2 install
    popd
    sudo rm -fr $BOOST
else
    $($boost_install)
fi

# yaml-cpp install
echo "Install yaml-cpp"
if [ -z "$yaml_cpp_install" ]; then
    YAMLCPP=yaml-cpp-0.5.1
    wget -qO- https://yaml-cpp.googlecode.com/files/${YAMLCPP}.tar.gz | tar -xzv
    pushd $YAMLCPP
    mkdir build && pushd build
    cmake ..
    sudo make
    sudo make install
    sudo rm -fr $YAMLCPP
    popd
    popd
else
    $($yaml_cpp_install)
fi

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

# Install Mongo C Driver
echo "Installing Mongo C Driver"
git clone -b master https://github.com/mongodb/mongo-c-driver
pushd mongo-c-driver/
git submodule update
./autogen.sh --with-libbson=bundled
make $c_flags && sudo make install
popd

# Install Mongo C++ Driver
echo "Installing Mongo C++ Driver"
git clone  -b master https://github.com/mongodb/mongo-cxx-driver.git
pushd mongo-cxx-driver/build
# This should be changed to a tag once the the code is stable
git checkout 9ae6f4d76c99a25f015f94cd8ac6b5f42d4e76d5
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
sudo make && sudo make install
popd

# workloadgen
echo "Building workgen"
#git clone git@github.com:10gen/CAP.git
pushd $workgenDir/build
git pull
cmake .. -DBOOST_ROOT=/usr/local
make
popd

# Run workloadgen
echo "Running the test_driver"
$workgenDir/build/src/test/test_driver
echo "Running sample1"
$workgenDir/build/mwg $workgenDir/examples/sample1.yml