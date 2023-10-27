# docker build -f .devcontainer/Dockerfile . -t genny-dev

FROM ubuntu:22.04

RUN apt -y update && apt -y install git wget gcc-12 g++-12 python3 python3-pip python3-venv libmongoc-dev build-essential cmake locales clangd libsnappy-dev

RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN wget https://github.com/bazelbuild/bazelisk/releases/download/v1.18.0/bazelisk-linux-arm64 -O /usr/local/bin/bazel && chmod 777 /usr/local/bin/bazel
RUN wget https://github.com/bazelbuild/buildtools/releases/download/v6.3.3/buildifier-linux-arm64 -O /usr/local/bin/buildifier && chmod 777 /usr/local/bin/buildifier

RUN python3 -m pip install click structlog click-option-group