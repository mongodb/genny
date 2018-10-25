#!/usr/bin/env bash

set -eou pipefail

usage() {
    echo "Usage:"
    echo ""
    echo "    $0 ActorName"
    echo ""
    echo "Creates and modifies boiler-plate necessary to create a new Actor."
    echo ""
}

create_header_text() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    echo "#ifndef HEADER_$uuid"
    echo "#define HEADER_$uuid"
    echo ""
    echo "#include <gennylib/Actor.hpp>"
    echo "#include <gennylib/PhaseLoop.hpp>"
    echo "#include <gennylib/context.hpp>"
    echo ""
    echo "namespace genny::actor {"
    echo ""
    echo "/**"
    echo " * TODO: document me"
    echo " */"
    echo "class $actor_name : public Actor {"
    echo ""
    echo "public:"
    echo "    explicit $actor_name(ActorContext& context, const unsigned int thread);"
    echo "    ~$actor_name() = default;"
    echo ""
    echo "    void run() override;"
    echo ""
    echo "    static ActorVector producer(ActorContext& context);"
    echo ""
    echo "private:"
    echo "    struct PhaseConfig;"
    echo "    PhaseLoop<PhaseConfig> _loop;"
    echo "};"
    echo ""
    echo "}  // namespace genny::actor"
    echo ""
    echo "#endif  // HEADER_$uuid"
}

create_impl_text() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    echo "#include <memory>"
    echo ""
    echo "#include <gennylib/actors/${actor_name}.hpp>"
    echo ""
    echo "namespace {"
    echo ""
    echo "}  // namespace"
    echo ""
    echo "struct genny::actor::${actor_name}::PhaseConfig {"
    echo "    PhaseConfig(PhaseContext& context, int thread)"
    echo "    {}"
    echo "};"
    echo ""
    echo "void genny::actor::${actor_name}::run() {"
    echo "    for (auto&& [phase, config] : _loop) {"
    echo "        for (auto&& _ : config) {"
    echo "            // TODO: main logic"
    echo "        }"
    echo "    }"
    echo "}"
    echo ""
    echo "genny::actor::${actor_name}::${actor_name}(genny::ActorContext& context, const unsigned int thread)"
    echo "    : _loop{context, thread} {}"
    echo ""
    echo "genny::ActorVector genny::actor::${actor_name}::producer(genny::ActorContext& context) {"
    echo "    auto out = std::vector<std::unique_ptr<genny::Actor>>{};"
    echo "    if (context.get<std::string>(\"Type\") != \"${actor_name}\") {"
    echo "        return out;"
    echo "    }"
    echo "    auto threads = context.get<int>(\"Threads\");"
    echo "    for (int i = 0; i < threads; ++i) {"
    echo "        out.push_back(std::make_unique<genny::actor::${actor_name}>(context, i));"
    echo "    }"
    echo "    return out;"
    echo "}"
}

create_header() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    create_header_text "$@" > "$(dirname "$0")/src/gennylib/include/gennylib/actors/${actor_name}.hpp"
}

create_impl() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    create_impl_text "$@" > "$(dirname "$0")/src/gennylib/src/actors/${actor_name}.cpp"
}

recreate_driver_file() {
    local uuid
    local actor_name
    local driver_file
    uuid="$1"
    actor_name="$2"
    driver_file="$(dirname "$0")/src/driver/src/DefaultDriver.cpp"

    < "$driver_file" \
      perl -pe "s|(// NextActorHeaderHere)|#include <gennylib/actors/${actor_name}.hpp>\\n\$1|" \
    | perl -pe "s|((\\s+)// NextActorProducerHere)|\$2genny::makeThreadedProducer(&genny::actor::${actor_name}::producer),\\n\$1|" \
    > "$$.driver.cpp"

    mv "$$.driver.cpp" "$driver_file"
}

recreate_gennylib_cmake_file() {
    local uuid
    local actor_name
    local cmake_file
    uuid="$1"
    actor_name="$2"
    cmake_file="$(dirname "$0")/src/gennylib/CMakeLists.txt"

    < "$cmake_file" \
    perl -pe "s|((\\s+)# ActorsEnd)|\$2src/actors/${actor_name}.cpp\\n\$1|" \
    > "$$.cmake.txt"

    mv "$$.cmake.txt" "$cmake_file"
}

if [[ "$#" != 1 ]]; then
    usage
    exit 1
fi

actor_name="$1"
if [ -z "$actor_name" ]; then
    usage
    exit 2
fi

uuid="$(uuidgen | sed s/-/_/g)"

create_header                "$uuid" "$actor_name"
create_impl                  "$uuid" "$actor_name"
recreate_driver_file         "$uuid" "$actor_name"
recreate_gennylib_cmake_file "$uuid" "$actor_name"
