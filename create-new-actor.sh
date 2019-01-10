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
    echo "#include <string_view>"
    echo ""
    echo "#include <mongocxx/pool.hpp>"
    echo ""
    echo "#include <gennylib/Actor.hpp>"
    echo "#include <gennylib/DefaultRandom.hpp>"
    echo "#include <gennylib/ExecutionStrategy.hpp>"
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
    echo "    ExecutionStrategy _strategy;"
    echo "    mongocxx::pool::entry _client;"
    echo "    genny::DefaultRandom _rng;"
    echo ""
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
    echo "#include <yaml-cpp/yaml.h>"
    echo ""
    echo "#include <mongocxx/client.hpp>"
    echo "#include <mongocxx/collection.hpp>"
    echo "#include <mongocxx/database.hpp>"
    echo ""
    echo "#include <boost/log/trivial.hpp>"
    echo ""
    echo "#include <bsoncxx/json.hpp>"
    echo "#include <gennylib/Cast.hpp>"
    echo "#include <gennylib/context.hpp>"
    echo "#include <gennylib/value_generators.hpp>"
    echo "#include <boost/log/trivial.hpp>"
    echo ""
    echo "#include <gennylib/value_generators.hpp>"
    echo "#include <gennylib/ExecutionStrategy.hpp>"
    echo ""
    echo ""
    echo "namespace genny::actor {"
    echo ""
    echo "struct ${actor_name}::PhaseConfig {"
    echo "    mongocxx::collection collection;"
    echo "    std::unique_ptr<value_generators::DocumentGenerator> docGen;"
    echo "    ExecutionStrategy::RunOptions options;"
    echo ""
    echo "    PhaseConfig(PhaseContext& phaseContext, genny::DefaultRandom& rng, const mongocxx::database& db)"
    echo "        : collection{db[phaseContext.get<std::string>(\"Collection\")]},"
    echo "          docGen{value_generators::makeDoc(phaseContext.get(\"Document\"), rng)},"
    echo "          options{ExecutionStrategy::getOptionsFrom(phaseContext, \"ExecutionsStrategy\")} {}"
    echo "};"
    echo ""
    echo "void ${actor_name}::run() {"
    echo "    for (auto&& config : _loop) {"
    echo "        for (const auto&& _ : config) {"
    echo "            _strategy.run("
    echo "                [&]() {"
    echo "                    // TODO: main logic"
    echo "                    bsoncxx::builder::stream::document mydoc{};"
    echo "                    auto view = config->docGen->view(mydoc);"
    echo "                    BOOST_LOG_TRIVIAL(info) << \" ${actor_name} Inserting \" << bsoncxx::to_json(view);"
    echo "                    config->collection.insert_one(view);"
    echo "                },"
    echo "                config->options);"
    echo "        }"
    echo ""
    echo "        _strategy.recordMetrics();"
    echo "    }"
    echo "}"
    echo ""
    echo "${actor_name}::${actor_name}(genny::ActorContext& context)"
    echo "    : Actor(context),"
    echo "      _rng{context.workload().createRNG()},"
    echo "      _strategy{context, ${actor_name}::id(), \"insert\"},"
    echo "      _client{std::move(context.client())},"
    echo "      _loop{context, _rng, (*_client)[context.get<std::string>(\"Database\")]} {}"
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

create_test() {
    local actor_name
    actor_name="$1"

    cat << EOF > "$(dirname "$0")/src/gennylib/test/${actor_name}_test.cpp"
#include "test.h"

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <MongoTestFixture.hpp>
#include <ActorHelper.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture, "${actor_name} successfully connects to a MongoDB instance.",
          "[standalone][single_node_replset][three_node_replset][sharded]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    YAML::Node config = YAML::Load(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: ${actor_name}
          Type: ${actor_name}
          Database: mydb
          ExecutionStrategy:
            ThrowOnFailure: true
          Phases:
          - Repeat: 100
            Collection: mycoll
            Document: {foo: {\$randomint: {min: 0, max: 100}}}
    )");


    SECTION("Inserts three documents into the database.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::kConnectionString.to_string());
            ah.run([](const genny::WorkloadContext& wc) { wc.actors()[0]->run(); });

            auto builder = bson_stream::document{};
            builder << "foo" << bson_stream::open_document
                    << "\$gte" << "0" << bson_stream::close_document
                    << bson_stream::finalize;

            auto count = db.collection("mycoll").count_documents(builder.view());
            // TODO: fixme
            REQUIRE(count == 101);
        } catch (const std::exception& e) {
            auto diagInfo = boost::diagnostic_information(e);
            INFO("CAUGHT " << diagInfo);
            FAIL(diagInfo);
        }
    }
}
} // namespace
EOF
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

recreate_gennylib_cmake_file() {
    local uuid_tag
    local actor_name
    local cmake_file
    uuid="$1"
    actor_name="$2"
    cmake_file="$(dirname "$0")/src/gennylib/CMakeLists.txt"

    < "$cmake_file" \
    perl -pe "s|((\\s+)# ActorsTestEnd)|\$2test/${actor_name}_test.cpp\\n\$1|" \
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
create_test                  "$actor_name"
recreate_gennylib_cmake_file "$uuid_tag" "$actor_name"
