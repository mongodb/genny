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
cat << EOF
Usage:

    $0 ActorName

Creates and modifies boiler-plate necessary to create a new Actor.
EOF
}

create_header_text() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

    cat << EOF
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

#ifndef $uuid_tag
#define $uuid_tag

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * TODO: document me
 *
 * Owner: TODO (which github team owns this Actor?)
 */
class $actor_name : public Actor {

public:
    explicit $actor_name(ActorContext& context);
    ~$actor_name() = default;

    static std::string_view defaultName() {
        return "$actor_name";
    }

    void run() override;

private:
    mongocxx::pool::entry _client;

    genny::metrics::Operation _totalInserts;

    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // $uuid_tag
EOF
}

create_impl_text() {
    local uuid_tag
    local actor_name
    uuid_tag="$1"
    actor_name="$2"

cat << EOF
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

#include <cast_core/actors/${actor_name}.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

struct ${actor_name}::PhaseConfig {
    mongocxx::collection collection;
    DocumentGenerator documentExpr;

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db, ActorId id)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          documentExpr{phaseContext.createDocumentGenerator(id, "Document")} {}
};

void ${actor_name}::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto inserts = _totalInserts.start();
            // TODO: main logic
            auto document = config->documentExpr();
            BOOST_LOG_TRIVIAL(info) << " ${actor_name} Inserting "
                                    << bsoncxx::to_json(document.view());
            config->collection.insert_one(document.view());
            inserts.addDocuments(1);
            inserts.addBytes(document.view().length());
            inserts.success();
        }
    }
}

${actor_name}::${actor_name}(genny::ActorContext& context)
    : Actor(context),
      _totalInserts{context.operation("Insert", ${actor_name}::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context.get<std::string>("Database")], ${actor_name}::id()} {}

namespace {
auto register${actor_name} = Cast::registerDefault<${actor_name}>();
}  // namespace
}  // namespace genny::actor
EOF
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

cat << EOF
🧞‍♂️ Successfully generated Actor skeleton for ${actor_name}:

EOF

git status --porcelain=v1 | sed 's/^/    /'

cat << EOF

Build and test ${actor_name} with the following command:

    ./scripts/lamp
    ./scripts/lamp resmoke-test --suites src/resmokeconfig/genny_standalone.yml
    ./scripts/lamp cmake-test

Run your workload as follows:

    ./dist/bin/genny run                                         \\
        --workload-file       ./workloads/docs/${actor_name}.yml \\
        --metrics-format      csv                                \\
        --metrics-output-file build/genny-metrics.csv            \\
        --mongo-uri           'mongodb://localhost:27017'

EOF
