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
q='`'

usage() {
cat << EOF
Usage:

    $0 ActorName

Create a new Actor header, implementation, documentation workload yaml,
and integration-style automated-test.
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
 * Indicate what the Actor does and give an example yaml configuration.
 * Markdown is supported in all docstrings so you could list an example here:
 *
 * ${q}${q}${q}yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: ${actor_name}
 *   Type: ${actor_name}
 *   Phases:
 *   - Document: foo
 * ${q}${q}${q}
 *
 * Or you can fill out the generated workloads/docs/${actor_name}.yml
 * file with extended documentation. If you do this, please mention
 * that extended documentation can be found in the docs/${actor_name}.yml
 * file.
 *
 * Owner: TODO (which github team owns this Actor?)
 */
class $actor_name : public Actor {

    //
    // This generated Actor does a simple ${q}collection.insert_one()${q} operation.
    // You may need to add a few private fields to this header file, but most
    // of the work is in the associated ${q}${actor_name}.cpp${q} file and its
    // assocated ${q}${actor_name}_test.cpp${q} integration-test file.
    //

public:
    //
    // The ActorContext exposes the workload YAML as well as other
    // collaborators. More details and examples are given in the
    // .cpp file.
    //
    explicit $actor_name(ActorContext& context);
    ~$actor_name() = default;

    //
    // Genny starts all Actor instances in their own threads and waits for all
    // the ${q}run()${q} methods to complete and that's "all there is".
    //
    // To help Actors coordinate, however, there is a built-in template-type
    // called ${q}genny::PhaseLoop${q}. All Actors that use ${q}PhaseLoop${q} will be run
    // in "lock-step" within Phases. It is recommended but not required to use
    // PhaseLoop. See further explanation in the .cpp file.
    //
    void run() override;

    //
    // This is how Genny knows that ${q}Type: $actor_name${q} in workload YAMLs
    // corresponds to this Actor class. It it also used by
    // the ${q}genny list-actors${q} command. Typically this should be the same as the
    // class name.
    //
    static std::string_view defaultName() {
        return "$actor_name";
    }

private:

    //
    // Each Actor can get its own connection from a number of connection-pools
    // configured in the ${q}Clients${q} section of the workload yaml. Since each
    // Actor is its own thread, there is no need for you to worry about
    // thread-safety in your Actor's internals. You likely do not need to have
    // more than one connection open per Actor instance but of course you do
    // you™️.
    //
    mongocxx::pool::entry _client;

    //
    // Your Actor can record an arbitrary number of different metrics which are
    // tracked by the ${q}metrics::Operation${q} type. This skeleton Actor does a
    // simple ${q}insert_one${q} operation so the name of this property corresponds
    // to that. Rename this and/or add additional ${q}metrics::Operation${q} types if
    // you do more than one thing. In addition, you may decide that you want
    // to support recording different metrics in different Phases in which case
    // you can remove this from the class and put it in the ${q}PhaseConfig${q} struct,
    // discussed in the .cpp implementation.
    //
    genny::metrics::Operation _totalInserts;

    //
    // The below struct and PhaseConfig are discussed in depth in the
    // ${q}${actor_name}.cpp${q} implementation file.
    //
    // Note that since ${q}PhaseLoop${q} uses pointers internally you don't need to
    // define anything about this type in this header, it just needs to be
    // pre-declared. The ${q}@private${q} docstring is to prevent doxygen from
    // showing your Actor's internals on the genny API docs website.
    //
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
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

//
// The ${q}PhaseLoop<PhaseConfig>${q} type constructs one ${q}PhaseConfig${q} instance
// for each ${q}Phase:${q} block in your Actor's YAML. We do this at
// Actor/Workload setup time and before we start recording real metrics.
// This lets you do any complicated or costly per-Phase setup operations
// (such as interacting with YAML or doing syntax validations) without
// impacting your metrics.
//
// Imagine you have the following YAML:
//
//     SchemaVersion: 2017-07-01
//     Actors:
//     - Name: ${actor_name}
//       Type: ${actor_name}
//       Threads: 100
//       Database: test
//       Phases:
//       - Collection: forPhase0
//         Document: {a: 7}
//         Duration: 1 minute
//       - Collection: forPhase1
//         Document: {b: 7}
//         Repeat: 100
//
// We'll automatically construct 2 ${q}PhaseConfig${q}s.
//
// You can pass additional parameters to your ${q}PhaseConfig${q} type by adding
// them to the ${q}_loop{}${q} initializer in the ${q}${actor_name}${q} constructor
// below. The first constructor parameter must always be a ${q}PhaseContext&${q}
// which lets you access the per-Phase configuration. In this example, the
// constructor also requires a ${q}database&${q} and the ${q}ActorId${q} which we pass
// along in the initializer.
//
// For more information on the Duration and Repeat keywords and how multiple Actors
// coordinate across Phases, see the extended example workload
// ${q}src/workloads/docs/HelloWorld-MultiplePhases.yml${q}.
//
// (${q}WorkloadContext${q}, ${q}ActorContext${q}, and ${q}PhaseContext${q} are all defined
// in ${q}context.hpp${q} - see full documentation there.)
//
// Within your Actor's ${q}run()${q} method, defined below, we iterate over the
// ${q}PhaseLoop<PhaseConfig> _loop${q} variable, and you can access the
// ${q}PhaseConfig${q} instance constructed for the current Phase via the
// ${q}config${q} variable using ${q}->${q}:
//
//     for (auto&& config : _loop) {
//         for (const auto&& _ : config) {
//             // this will be forPhase0 during Phase 0
//             // and forPhase1 during Phase 1
//             auto collection = config->collection;
//         }
//     }
//
// The inner loop ${q}for(const auto&& _ : config)${q} is how PhaseLoop keeps
// running your code during the course of the current Phase. In the above
// example, ${q}PhaseLoop${q} will see that Phase 0 is supposed to run for 1
// minute so it will continue running the body of the inner loop for 1
// minute before it will signal to the Genny internals that it is done with
// the current Phase. Once all Actors indicate that they are done with the
// current Phase, Genny lets Actors proceed with the next Phase by
// advancing to the next ${q}config${q} instance in the outer loop.
//
// See the full documentation in ${q}PhaseLoop.hpp${q}.
//

struct ${actor_name}::PhaseConfig {
    mongocxx::collection collection;

    //
    // DocumentGenerator is a powerful mini-templating-engine that lets you
    // generate random data at runtime using a simple templating language. The
    // best way to learn it is to look at a few examples in the ${q}src/workloads${q}
    // directory. Here's a simple example:
    //
    //     #...
    //     Phases:
    //     - Phase: 0
    //       Document: {a: {^RandomInt: {min: 0, max: 100}}}
    //
    // When we construct a ${q}DocumentGenerator${q} from this:
    //
    //     auto docGen = phaseContext.createDocumentGenerator(id, "Document");
    //
    // We can evaluate it multiple times to get a randomly-generated document:
    //
    //     bsoncxx::document::value first = docGen();  // => {a: 27}
    //     bsoncxx::document::value second = docGen(); // => {a: 34}
    //
    // All document-generators are deterministically seeded so all runs of the
    // same workload produce the same set of documents.
    //
    DocumentGenerator documentExpr;


    //
    // When Genny constructs each Actor it assigns it a unique ID. This is used
    // by many of the ${q}PhaseContext${q} and ${q}ActorContext${q} methods. This is becase
    // we construct only one ${q}ActorContext${q} for the entire ${q}Actor:${q} block even
    // if we're constructing 100 instances of the same Actor type. See the full
    // documentation in ${q}context.hpp${q}.
    //

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db, ActorId id)
        : collection{db[phaseContext["Collection"].to<std::string>()]},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id} {}
};


//
// Genny spins up a thread for each Actor instance. The ${q}Threads:${q} configuration
// tells Genny how many such instances to create. See further documentation in
// the ${q}Actor.hpp${q} file.
//
void ${actor_name}::run() {
    //
    // The ${q}config${q} variable is bound to the ${q}PhaseConfig${q} that was
    // constructed for the current Phase at setup time. This loop will automatically
    // block, wait, and repeat at the right times to gracefully coordinate with other
    // Actors so that all Actors start and end at the right time.
    //
    for (auto&& config : _loop) {
        //
        // This inner loop is run according to the Phase configuration for this
        // Actor. If you have ${q}{Duration: 1 minute}${q} this loop will be run
        // for one minute, etc. It also handles rate-limiting and error-handling
        // as discussed below.
        //
        // The body of the below "inner" loop is where most of your logic should start.
        // Alternatively you may decide to put some logic in your ${q}PhaseConfig${q} type
        // similarly to what the ${q}CrudActor${q} does. If your Actor is simple and
        // only supports a single type of operation, it is easiest to put your logic in
        // the body of the below loop.
        //
        for (const auto&& _ : config) {
            //
            // Evaluate the DocumentGenerator template:
            //
            auto document = config->documentExpr();

            //
            // The clock starts running only when you call ${q}.start()${q}. The returned object
            // from this lets you record how many bytes, documents, and actual
            // iterations (usually 1) your Actor completes while the clock is running. You then
            // must stop the clock by calling either ${q}.success()${q} if everything is okay
            // or ${q}.fail()${q} if there was a problem. It's undefined behavior what will
            // happen if you don't stop the metrics clock (e.g. if there is an uncaught
            // exception). See the full documentation on ${q}OperationContext${q}.
            //
            // We don't care about how long the value-generator takes to run
            // so we don't start the clock until after evaluating it. Purists
            // would argue we even start until after we've done the below log
            // statement. You decide.
            //
            auto inserts = _totalInserts.start();

            //
            // You have the full power of boost logging which is rendered to stdout.
            // Convention is to log generated documents and "normal" events at the
            // debug level.
            //
            BOOST_LOG_TRIVIAL(debug) << " ${actor_name} Inserting "
                                    << bsoncxx::to_json(document.view());

            //
            // ⚠️ If your Actor throws any uncaught exceptions, the whole Workload will
            // attempt to end as quickly as possible. Every time this inner loop loops
            // around, it checks for any other Actors' exceptions and will stop iterating
            // if it sees any. Such failed workloads are considered "programmer error"
            // and will mark the workload task as a system-failure in Evergreen. ⚠️
            //

            //
            // Actually do the logic.
            //
            // We expect that this insert operation may actually fail sometimes and we
            // wouldn't consider this to be programmer-error (or a configuration-
            // error) so we catch try/catch with the expected exception types.
            //
            try {

                config->collection.insert_one(document.view());

                inserts.addDocuments(1);
                inserts.addBytes(document.view().length());
                inserts.success();
            } catch(mongocxx::operation_exception& e) {
                inserts.failure();
                //
                // MongoException lets you include a "causing" bson document in the
                // exception message for help debugging.
                //
                BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            }
        }
    }
}

${actor_name}::${actor_name}(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", ${actor_name}::id())},
      _client{context.client()},
      //
      // Pass any additional constructor parameters that your ${q}PhaseConfig${q} needs.
      //
      // The first argument passed in here is actually the ${q}ActorContext${q} but the
      // ${q}PhaseLoop${q} reads the ${q}PhaseContext${q}s from there and constructs one
      // instance for each Phase.
      //
      _loop{context, (*_client)[context["Database"].to<std::string>()], ${actor_name}::id()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
//
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
#
# For more information on how to configure Repeat/Duration/*
# see extended example file
# ${q}src/workloads/docs/HelloWorld-MultiplePhases.yml${q}

Actors:
- Name: ${actor_name}
  Type: ${actor_name}
  Threads: 100
  Database: test
  Phases:
  - Phase: 0
    Repeat: 1e3 # used by PhaesLoop
    # below used by PhaseConfig in ${actor_name}.cpp
    Collection: test
    Document: {foo: {^RandomInt: {min: 0, max: 100}}}
  - Phase: 1
    Duration: 5 seconds
    Collection: test
    Document: {bar: {^RandomInt: {min: 500, max: 10000}}}
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

namespace genny {
namespace {
using namespace genny::testing;
namespace bson_stream = bsoncxx::builder::stream;

//
// ⚠️ There is a "known" failure that you should find and fix as a bit of
// an exercise in reading and testing your Actor. ⚠️
//

TEST_CASE_METHOD(MongoTestFixture, "${actor_name} successfully connects to a MongoDB instance.",
          "[standalone][single_node_replset][three_node_replset][sharded][${actor_name}]") {

    dropAllDatabases();
    auto db = client.database("mydb");

    NodeSource nodes = NodeSource(R"(
        SchemaVersion: 2018-07-01
        Actors:
        - Name: ${actor_name}
          Type: ${actor_name}
          Database: mydb
          Phases:
          - Repeat: 100
            Collection: mycoll
            Document: {foo: {^RandomInt: {min: 0, max: 100}}}
    )", __FILE__);


    SECTION("Inserts documents into the database.") {
        try {
            genny::ActorHelper ah(nodes.root(), 1, MongoTestFixture::connectionUri().to_string());
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
}  // namespace
}  // namespace genny
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
🧞‍ Successfully generated Actor skeleton for ${actor_name}:

git status:

EOF

git status --porcelain=v1 | sed 's/^/    /'

cat << EOF

Build and test ${actor_name} with the following command:

    ./scripts/lamp
    ./scripts/lamp cmake-test
    ./scripts/get-mongo-source.sh
    ./scripts/lamp resmoke-test --suites src/resmokeconfig/genny_standalone.yml

The resmoke-test will fail because there is a "hidden" bug in the generated
integration-test-case that you are expected to find as a part of reading through
the generated code.

Run your workload as follows:

    ./dist/bin/genny run \\
        --workload-file       ./src/workloads/docs/${actor_name}.yml \\
        --metrics-format      csv \\
        --metrics-output-file build/genny-metrics.csv \\
        --mongo-uri           'mongodb://localhost:27017'

EOF
