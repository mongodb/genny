// Copyright 2019-present MongoDB Inc.
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

#ifndef HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED
#define HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <boost/log/trivial.hpp>
#include <boost/asio.hpp>

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
 * - Name: RollingCollectionWriter
 *   Type: RollingCollectionWriter
 *   Phases:
 *   - Document: foo
 * ```
 *
 * Or you can fill out the generated workloads/docs/RollingCollectionWriter.yml
 * file with extended documentation. If you do this, please mention
 * that extended documentation can be found in the docs/RollingCollectionWriter.yml
 * file.
 *
 * Owner: Storage Engines
 */
class RollingCollectionWriter : public Actor {
public:
    explicit RollingCollectionWriter(ActorContext& context);
    ~RollingCollectionWriter() = default;
    void run() override;
    static std::string_view defaultName() {
        return "RollingCollectionWriter";
    }

private:
    mongocxx::pool::entry _client;
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
    int64_t _collectionWindowSize;
    boost::asio::deadline_timer _timer;
};

struct RollingCollectionWindow {
    RollingCollectionWindow(int64_t _collectionWindowSize)
        : lastSecond{INT64_MAX},
          max{_collectionWindowSize - 1},
          min{0},
          collectionWindowSize{_collectionWindowSize} { }
    int64_t collectionWindowSize;
    int64_t lastSecond;
    // Max collection id
    int64_t max;
    // Min collection id
    int64_t min;
};
bool updateCurrentIdWindow(RollingCollectionWindow& config);

}  // namespace genny::actor

#endif  // HEADER_34CFFB35_C0EB_42FA_ACF2_91A7E6556841_INCLUDED
