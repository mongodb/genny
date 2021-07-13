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

#ifndef HEADER_9009943B_4404_489C_93CB_B09EC123E951_INCLUDED
#define HEADER_9009943B_4404_489C_93CB_B09EC123E951_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Prepares a database for testing. For use with `MultiCollectionUpdate` and
 * `MultiCollectionQuery` actors. It loads a set of documents into multiple collections with
 * indexes. Each collection is identically configured. The document shape, number of documents,
 * number of collections, and list of indexes are all adjustable from the yaml configuration.
 *
 * This actor is identical to `Loader` except it loads with monotonically increasing _ids and it has
 * an additional pair of arguments ('FieldIncreasingByOffsetFromID' and 'OffsetFromID') to generate
 * a field in each document whose value is 'OffsetFromID' greater than the '_id' of the document.
 * For example, if the field name is 'a' and the offset is 1, the documents will look like
 *      {
 *          "_id": 1,
 *          "a": 2,
 *          ...
 *      },
 *      {
 *          "_id": 2,
 *          "a": 3,
 *          ...
 *      }, ...
 *
 * TODO: This specialized actor should be removed once the current value generator supports
 * monotonically increasing values.
 *
 * Owner: Storage Engines
 */
class MonotonicLoader : public Actor {

public:
    explicit MonotonicLoader(ActorContext& context, uint thread);
    ~MonotonicLoader() override = default;

    static std::string_view defaultName() {
        return "MonotonicLoader";
    }
    void run() override;

private:
    /** @private */
    struct PhaseConfig;

    metrics::Operation _totalBulkLoad;
    metrics::Operation _individualBulkLoad;
    metrics::Operation _indexBuild;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_9009943B_4404_489C_93CB_B09EC123E951_INCLUDED
