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
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    echo "#ifndef $uuid_tag"
    echo "#define $uuid_tag"
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
    echo "    static std::string_view defaultName() {"
    echo "        return \"$actor_name\";"
    echo "    }"
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
    echo "#endif  // $uuid_tag"
}

create_impl_text() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    echo "#include <cast_core/actors/${actor_name}.hpp>"
    echo ""
    echo "#include <memory>"
    echo ""
    echo "namespace genny::actor {"
    echo ""
    echo "struct ${actor_name}::PhaseConfig {"
    echo "    PhaseConfig(PhaseContext& context) {}"
    echo "};"
    echo ""
    echo "void ${actor_name}::run() {"
    echo "    for (auto&& config : _loop) {"
    echo "        for (auto&& _ : config) {"
    echo "            // TODO: main logic"
    echo "        }"
    echo "    }"
    echo "}"
    echo ""
    echo "${actor_name}::${actor_name}(ActorContext& context)"
    echo "    : Actor(context),"
    echo "      _loop{context} {}"
    echo ""
    echo "namespace {"
    echo "auto register${actor_name} = Cast::registerDefault<${actor_name}>();"
    echo "}  // namespace"
    echo "}  // namespace genny::actor"
}

create_header() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    create_header_text "$@" > "$(dirname "$0")/src/cast_core/include/cast_core/actors/${actor_name}.hpp"
}

create_impl() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    create_impl_text "$@" > "$(dirname "$0")/src/cast_core/src/actors/${actor_name}.cpp"
}

recreate_cast_core_cmake_file() {
    local uuid_tag
    local actor_name
    local cmake_file
    uuid_tag="$1"
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

uuid_tag="$("$(dirname "$0")/generate-uuid-tag.sh")"

create_header                "$uuid_tag" "$actor_name"
create_impl                  "$uuid_tag" "$actor_name"
recreate_cast_core_cmake_file "$uuid_tag" "$actor_name"
