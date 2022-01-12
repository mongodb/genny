// Copyright 2021-present MongoDB Inc.
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

#ifndef HEADER_8E9D612A_25BA_48F9_8930_3F7FBA436E1B_INCLUDED
#define HEADER_8E9D612A_25BA_48F9_8930_3F7FBA436E1B_INCLUDED

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
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: SampleHttpClient
 *   Type: SampleHttpClient
 *   Phases:
 *   - Document: foo
 * ```
 *
 * Or you can fill out the generated workloads/docs/SampleHttpClient.yml
 * file with extended documentation. If you do this, please mention
 * that extended documentation can be found in the docs/SampleHttpClient.yml
 * file.
 *
 * Owner: TODO (which github team owns this Actor?)
 */
class SampleHttpClient : public Actor {

    //
    // This generated Actor does a simple `collection.insert_one()` operation.
    // You may need to add a few private fields to this header file, but most
    // of the work is in the associated `SampleHttpClient.cpp` file and its
    // assocated `SampleHttpClient_test.cpp` integration-test file.
    //

public:
    //
    // The ActorContext exposes the workload YAML as well as other
    // collaborators. More details and examples are given in the
    // .cpp file.
    //
    explicit SampleHttpClient(ActorContext& context);
    ~SampleHttpClient() = default;

    //
    // Genny starts all Actor instances in their own threads and waits for all
    // the `run()` methods to complete and that's "all there is".
    //
    // To help Actors coordinate, however, there is a built-in template-type
    // called `genny::PhaseLoop`. All Actors that use `PhaseLoop` will be run
    // in "lock-step" within Phases. It is recommended but not required to use
    // PhaseLoop. See further explanation in the .cpp file.
    //
    void run() override;

    //
    // This is how Genny knows that `Type: SampleHttpClient` in workload YAMLs
    // corresponds to this Actor class. It it also used by
    // the `genny list-actors` command. Typically this should be the same as the
    // class name.
    //
    static std::string_view defaultName() {
        return "SampleHttpClient";
    }

private:

    //
    // Each Actor can get its own connection from a number of connection-pools
    // configured in the `Clients` section of the workload yaml. Since each
    // Actor is its own thread, there is no need for you to worry about
    // thread-safety in your Actor's internals. You likely do not need to have
    // more than one connection open per Actor instance but of course you do
    // you™️.
    //
    mongocxx::pool::entry _client;

    //
    // Your Actor can record an arbitrary number of different metrics which are
    // tracked by the `metrics::Operation` type. This skeleton Actor does a
    // simple `insert_one` operation so the name of this property corresponds
    // to that. Rename this and/or add additional `metrics::Operation` types if
    // you do more than one thing. In addition, you may decide that you want
    // to support recording different metrics in different Phases in which case
    // you can remove this from the class and put it in the `PhaseConfig` struct,
    // discussed in the .cpp implementation.
    //
    genny::metrics::Operation _totalInserts;

    //
    // The below struct and PhaseConfig are discussed in depth in the
    // `SampleHttpClient.cpp` implementation file.
    //
    // Note that since `PhaseLoop` uses pointers internally you don't need to
    // define anything about this type in this header, it just needs to be
    // pre-declared. The `@private` docstring is to prevent doxygen from
    // showing your Actor's internals on the genny API docs website.
    //
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_8E9D612A_25BA_48F9_8930_3F7FBA436E1B_INCLUDED
