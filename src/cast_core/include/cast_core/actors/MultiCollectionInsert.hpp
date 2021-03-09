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

#ifndef HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED
#define HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED

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
 * - Name: MultiCollectionInsert
 *   Type: MultiCollectionInsert
 *   Phases:
 *   - Document: foo
 * ```
 *
 * Or you can fill out the generated workloads/docs/MultiCollectionInsert.yml
 * file with extended documentation. If you do this, please mention
 * that extended documentation can be found in the docs/MultiCollectionInsert.yml
 * file.
 *
 * Owner: TODO (which github team owns this Actor?)
 */
class MultiCollectionInsert : public Actor {
public:
    explicit MultiCollectionInsert(ActorContext& context);
    ~MultiCollectionInsert() = default;

    static std::string_view defaultName() {
        return "MultiCollectionInsert";
    }
    void run() override;


private:
    /** @private */
    struct PhaseConfig;
    genny::DefaultRandom _rng;

    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_5C95E113_3356_448E_9BB9_5997F62D40FE_INCLUDED
