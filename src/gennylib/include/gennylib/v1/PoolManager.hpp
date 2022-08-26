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

#include <bsoncxx/types/bson_value/value.hpp>
#include <mongocxx/options/auto_encryption.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/Node.hpp>

namespace genny::v1 {

class EncryptionOptions;
class EncryptionContext {
public:
    EncryptionContext(const Node& encryptionOptsNode, std::string uri);
    ~EncryptionContext();

    std::pair<std::string, std::string> getKeyVaultNamespace() const;
    mongocxx::options::auto_encryption getAutoEncryptionOptions() const;

    void setupKeyVault();

    bsoncxx::document::value generateKMSProvidersDoc() const;
    bsoncxx::document::value generateSchemaMapDoc() const;
    bsoncxx::document::value generateExtraOptionsDoc() const;

private:
    std::unique_ptr<EncryptionOptions> _encryptionOpts;
    std::string _uri;
};

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
     *  Connection/query-string parameters can be added via `Clients` configuration
     *  passed in when calling `client()`.
     *  See PoolFactory for how this can be configured.
     *
     * @param callback
     *   a callback to be invoked for every `mongocxx::events::command_started_event`
     * @param dryRun
     *   whether the workload is a dry run. If true, setup that requires a connection
     *   to a server will not be run (e.g. setting up data keys for encryption).
     */
    PoolManager(OnCommandStartCallback callback, bool dryRun = false)
        : _apmCallback{std::move(callback)}, _dryRun(dryRun) {}

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
    /** callback passed into ctor */
    OnCommandStartCallback _apmCallback;

    struct Pools {
        std::shared_ptr<EncryptionContext> encryption;
        std::unordered_map<size_t, std::unique_ptr<mongocxx::pool>> instances;
    };
    // pair each map ↑ with a mutex for adding new pools
    using LockAndPools = std::pair<std::mutex, Pools>;

    /** the pools themselves */
    std::unordered_map<std::string, LockAndPools> _pools;
    /** lock on finding/creating the LockAndPools for a given string */
    std::mutex _poolsLock;

    /** whether setup that needs a server connection should be skipped on client pool creation */
    bool _dryRun;
};

}  // namespace genny::v1

#endif  // HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
