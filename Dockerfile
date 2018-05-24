FROM    ubuntu:14.04

RUN     apt-get update

RUN     apt-get install -y wget
RUN     apt-get install -y software-properties-common
RUN     apt-get install -y clang
RUN     apt-get install -y binutils
RUN     apt-get install -y make

RUN     apt-get install -y emacs
RUN     apt-get install -y vim

COPY    ./etc/docker /tmp/docker
RUN     /tmp/docker/install-cmake.sh
