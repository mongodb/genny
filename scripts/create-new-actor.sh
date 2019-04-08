#!/usr/bin/env bash

# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eou pipefail

year="$(date '+%Y')"

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

    echo "// Copyright ${year}-present MongoDB Inc."
    echo "//"
    echo "// Licensed under the Apache License, Version 2.0 (the \"License\");"
    echo "// you may not use this file except in compliance with the License."
    echo "// You may obtain a copy of the License at"
    echo "//"
    echo "// http://www.apache.org/licenses/LICENSE-2.0"
    echo "//"
    echo "// Unless required by applicable law or agreed to in writing, software"
    echo "// distributed under the License is distributed on an \"AS IS\" BASIS,"
    echo "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
    echo "// See the License for the specific language governing permissions and"
    echo "// limitations under the License."
    echo ""
    echo "#ifndef $uuid_tag"
    echo "#define $uuid_tag"
    echo ""
    echo "#include <string_view>"
    echo ""
    echo "#include <mongocxx/pool.hpp>"
    echo ""
    echo "#include <gennylib/Actor.hpp>"
    echo "#include <gennylib/ExecutionStrategy.hpp>"
    echo "#include <gennylib/PhaseLoop.hpp>"
    echo "#include <gennylib/context.hpp>"
    echo ""
    echo "namespace genny::actor {"
    echo ""
    echo "/**"
    echo " * TODO: document me"
    echo " *"
    echo " * Owner: TODO (which github team owns this Actor?)"
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
    echo ""
    echo "    /** @private */"
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

    echo "// Copyright ${year}-present MongoDB Inc."
    echo "//"
    echo "// Licensed under the Apache License, Version 2.0 (the \"License\");"
    echo "// you may not use this file except in compliance with the License."
    echo "// You may obtain a copy of the License at"
    echo "//"
    echo "// http://www.apache.org/licenses/LICENSE-2.0"
    echo "//"
    echo "// Unless required by applicable law or agreed to in writing, software"
    echo "// distributed under the License is distributed on an \"AS IS\" BASIS,"
    echo "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
    echo "// See the License for the specific language governing permissions and"
    echo "// limitations under the License."
    echo ""
    echo "#include <cast_core/actors/${actor_name}.hpp>"
    echo ""
    echo "#include <memory>"
    echo ""
    echo "#include <yaml-cpp/yaml.h>"
    echo ""
    echo "#include <bsoncxx/json.hpp>"
    echo "#include <mongocxx/client.hpp>"
    echo "#include <mongocxx/collection.hpp>"
    echo "#include <mongocxx/database.hpp>"
    echo ""
    echo "#include <boost/log/trivial.hpp>"
    echo ""
    echo "#include <gennylib/Cast.hpp>"
    echo "#include <gennylib/ExecutionStrategy.hpp>"
    echo "#include <gennylib/context.hpp>"
    echo ""
    echo "#include <value_generators/DocumentGenerator.hpp>"
    echo ""
    echo ""
    echo "namespace genny::actor {"
    echo ""
    echo "struct ${actor_name}::PhaseConfig {"
    echo "    mongocxx::collection collection;"
    echo "    DocumentGenerator documentExpr;"
    echo "    ExecutionStrategy::RunOptions options;"
    echo ""
    echo "    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db, ActorId id)"
    echo "        : collection{db[phaseContext.get<std::string>(\"Collection\")]},"
    echo "          documentExpr{phaseContext.createDocumentGenerator(id, \"Document\")},"
    echo "          options{ExecutionStrategy::getOptionsFrom(phaseContext, \"ExecutionsStrategy\")} {}"
    echo "};"
    echo ""
    echo "void ${actor_name}::run() {"
    echo "    for (auto&& config : _loop) {"
    echo "        for (const auto&& _ : config) {"
    echo "            _strategy.run("
    echo "                [&](metrics::OperationContext& ctx) {"
    echo "                    // TODO: main logic"
    echo "                    auto document = config->documentExpr();"
    echo "                    BOOST_LOG_TRIVIAL(info) << \" ${actor_name} Inserting \""
    echo "                                            << bsoncxx::to_json(document.view());"
    echo "                    config->collection.insert_one(document.view());"
    echo "                    ctx.addDocuments(1);"
    echo "                    ctx.addBytes(document.view().length());"
    echo "                },"
    echo "                config->options);"
    echo "        }"
    echo "    }"
    echo "}"
    echo ""
    echo "${actor_name}::${actor_name}(genny::ActorContext& context)"
    echo "    : Actor(context),"
    echo "      _strategy{context.operation(\"insert\", ${actor_name}::id())},"
    echo "      _client{std::move(context.client())},"
    echo "      _loop{context, (*_client)[context.get<std::string>(\"Database\")], ${actor_name}::id()} {}"
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

    create_header_text "$@" > "$(dirname "$0")/../src/cast_core/include/cast_core/actors/${actor_name}.hpp"
}

create_impl() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    create_impl_text "$@" > "$(dirname "$0")/../src/cast_core/src/${actor_name}.cpp"
}

create_workload_yml() {
    local actor_name
    actor_name="$1"
cat << EOF > "$(dirname "$0")/../src/workloads/docs/${actor_name}.yml"
SchemaVersion: 2018-07-01
Owner: TODO put your github team name here e.g. @mongodb/stm

# TODO: delete this file or add a meaningful workload using or
#       demonstrating your Actor

Actors:
- Name: ${actor_name}
  Type: ${actor_name}
  Threads: 100
  Database: test
  Phases:
  - Phase: 0
    Repeat: 10 # used by PhaesLoop
    Retries: 7 # used by ExecutionStrategy
    # below used by PhaseConfig in ${actor_name}.cpp
    Collection: test
    Document: {foo: {^RandomInt: {min: 0, max: 100}}}
EOF
}

create_test() {
    local actor_name
    actor_name="$1"

    cat << EOF > "$(dirname "$0")/../src/cast_core/test/${actor_name}_test.cpp"
// Copyright ${year}-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <boost/exception/diagnostic_information.hpp>

#include <yaml-cpp/yaml.h>

#include <testlib/MongoTestFixture.hpp>
#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>

#include <gennylib/context.hpp>

namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

TEST_CASE_METHOD(MongoTestFixture, "${actor_name} successfully connects to a MongoDB instance.",
          "[standalone][single_node_replset][three_node_replset][sharded][${actor_name}]") {

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
            Document: {foo: {^RandomInt: {min: 0, max: 100}}}
    )");


    SECTION("Inserts documents into the database.") {
        try {
            genny::ActorHelper ah(config, 1, MongoTestFixture::connectionUri().to_string());
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
create_test                  "$actor_name"
create_workload_yml          "$actor_name"

echo "Successfully generated Actor skeleton for ${actor_name}:"
echo ""
git status --porcelain=v1 | sed 's/^/    /'
echo ""
echo "Build and test ${actor_name} with the following command:"
echo ""
echo "    ./scripts/lamp"
echo "    ./scripts/lamp resmoke-test --suites src/resmokeconfig/genny_standalone.yml"
echo "    ./scripts/lamp cmake-test"
echo ""
echo "Run your workload as follows:"
echo ""
echo "    ./dist/bin/genny run                                         \\"
echo "        --workload-file       ./workloads/docs/${actor_name}.yml \\"
echo "        --metrics-format      csv                                \\"
echo "        --metrics-output-file build/genny-metrics.csv            \\"
echo "        --mongo-uri           'mongodb://localhost:27017'"
echo ""
