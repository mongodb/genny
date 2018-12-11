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
    echo "    explicit $actor_name(ActorContext& context);"
    echo "    ~$actor_name() = default;"
    echo ""
    echo "    static std::string_view defaultName() { return \"$actor_name\"; }"
    echo ""
    echo "    void run() override;"
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
    echo "#include <cast_core/actors/${actor_name}.hpp>"
    echo ""
    echo "namespace genny {"
    echo ""
    echo "struct actor::${actor_name}::PhaseConfig {"
    echo "    PhaseConfig(PhaseContext& context) {}"
    echo "};"
    echo ""
    echo "void actor::${actor_name}::run() {"
    echo "    for (auto&& [phase, config] : _loop) {"
    echo "        for (auto&& _ : config) {"
    echo "            // TODO: main logic"
    echo "        }"
    echo "    }"
    echo "}"
    echo ""
    echo "actor::${actor_name}::${actor_name}(ActorContext& context)"
    echo "    : Actor(context),"
    echo "      _loop{context} {}"
    echo ""
    echo "namespace {"
    echo "auto register${actor_name} = Cast::makeDefaultRegistration<actor::${actor_name}>();"
    echo "}"
    echo "} // namespace genny"
}

create_header() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    create_header_text "$@" > "$(dirname "$0")/src/cast_core/include/cast_core/actors/${actor_name}.hpp"
}

create_impl() {
    local uuid
    local actor_name
    uuid="$1"
    actor_name="$2"

    create_impl_text "$@" > "$(dirname "$0")/src/cast_core/src/actors/${actor_name}.cpp"
}

recreate_cast_core_cmake_file() {
    local uuid
    local actor_name
    local cmake_file
    uuid="$1"
    actor_name="$2"
    cmake_file="$(dirname "$0")/src/cast_core/CMakeLists.txt"

    < "$cmake_file" \
    perl -pe "s|((\\s+)# ActorsEnd)|\$2src/actors/${actor_name}.cpp\\n\$1|" \
    > "$$.cmake.txt"

    mv "$$.cmake.txt" "$cmake_file"
}

if [[ "$#" != 1 ]]; then
    usage
    exit 1
fi

case $1 in
  (-h|--help) usage; exit 0;;
esac

actor_name="$1"
if [ -z "$actor_name" ]; then
    usage
    exit 2
fi

uuid="$(uuidgen | sed s/-/_/g)"

create_header                "$uuid" "$actor_name"
create_impl                  "$uuid" "$actor_name"
recreate_cast_core_cmake_file "$uuid" "$actor_name"
