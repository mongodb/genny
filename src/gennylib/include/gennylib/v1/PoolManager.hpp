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

#ifndef HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
#define HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <mongocxx/pool.hpp>

#include <gennylib/Node.hpp>

namespace genny::v1 {

/**
 * A wrapper atop `PoolFactory` that manages a set of pools' lifecycles.
 */
class PoolManager {
public:
    /**
     * Apply this callback for every `mongocxx::events::command_started_event`
     * for all connections created from this PoolManager.
     */
    using OnCommandStartCallback =
        std::function<void(const mongocxx::events::command_started_event&)>;

    /**
     * @param mongoUri
     *  the base URI to use. This typically comes from the command-line argument.
     *  Additional connection/query-string parameters can be added via `Clients` configuration
     *  passed in when calling `client()`.
     *  See PoolFactory for how this can be configured.
     * keyword.
     *
     * @param callback
     *   a callback to be invoked for every `mongocxx::events::command_started_event`
     */
    PoolManager(std::string mongoUri, OnCommandStartCallback callback)
        : _mongoUri{std::move(mongoUri)}, _apmCallback{std::move(callback)} {}

    /**
     * Obtain a connection or throw if none available.
     *
     * This is allowed to be called from multiple threads simultaneously.
     *
     * @warning it is advised to only call this during setup since creating a connection pool
     * can be an expensive operation
     *
     * @param name
     *  the name of the pool to use. This corresponds to a key within the `Clients` configuration.
     * @param instance
     *   which instance of the named pool to use. Will be created on-demand the first time the
     *   (name,instance) pair is used.
     * @param context
     *   the WorkloadContext used to look up the configurations
     * @return a connection from the pool or throw if none available
     */
    mongocxx::pool::entry client(const std::string& name, size_t instance, const Node& context);

    // Only used for testing
    /** @private */
    std::unordered_map<std::string, size_t> instanceCount();

private:
    /** base uri passed into ctor */
    std::string _mongoUri;
    /** callback passed into ctor */
    OnCommandStartCallback _apmCallback;

    using Pools = std::unordered_map<size_t, std::unique_ptr<mongocxx::pool>>;
    // pair each map ↑ with a mutex for adding new pools
    using LockAndPools = std::pair<std::mutex, Pools>;

    /** the pools themselves */
    std::unordered_map<std::string, LockAndPools> _pools;
    /** lock on finding/creating the LockAndPools for a given string */
    std::mutex _poolsLock;
};

}  // namespace genny::v1

#endif  // HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
