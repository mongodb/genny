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

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/v1/PoolFactory.hpp>
#include <gennylib/v1/PoolManager.hpp>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/document.hpp>

namespace genny::v1 {
namespace {

auto createPool(const Node& clientNode,
                PoolManager::OnCommandStartCallback& apmCallback,
                EncryptionManager& encryptionManager) {

    auto mongoUri = clientNode["URI"].to<std::string>();

    auto poolFactory = PoolFactory(mongoUri, apmCallback);

    auto queryOpts = clientNode["QueryOptions"].maybe<std::map<std::string, std::string>>();
    if (queryOpts) {
        poolFactory.setOptions(PoolFactory::kQueryOption, *queryOpts);
    }

    auto accessOpts = clientNode["AccessOptions"].maybe<std::map<std::string, std::string>>();
    if (accessOpts) {
        poolFactory.setOptions(genny::v1::PoolFactory::kAccessOption, *accessOpts);
    }

    auto encryptOpts = clientNode["EncryptionOptions"].maybe<EncryptionOptions>();
    if (encryptOpts) {
        poolFactory.setEncryptionContext(
            encryptionManager.createEncryptionContext(poolFactory.makeUri(), *encryptOpts));
    }

    return poolFactory.makePool();
}

}  // namespace

}  // namespace genny::v1


mongocxx::pool::entry genny::v1::PoolManager::createClient(const std::string& name,
                                                           size_t instance,
                                                           const Node& workloadCtx) {
    // Only one thread can access pools.operator[] at a time...
    std::unique_lock<std::mutex> getLock{this->_poolsLock};
    LockAndPools& lap = this->_pools[name];

    if (!_encryptionManager) {
        _encryptionManager = std::make_unique<EncryptionManager>(workloadCtx, _dryRun);
    }

    // ...but no need to keep the lock open past this.
    // Two threads trying access createClient("foo",0) at the same
    // time will subsequently block on the unique_lock.
    getLock.unlock();

    // only one thread can access the map of pools at once
    std::unique_lock<std::mutex> lock{lap.first};

    Pools& pools = lap.second;

    auto& clientNode = workloadCtx["Clients"][name];
    bool shouldPrewarm = !_dryRun && !clientNode["NoPreWarm"].maybe<bool>().value_or(false);
    auto& pool = pools[instance];
    if (pool == nullptr) {
        pool = createPool(clientNode, this->_apmCallback, *_encryptionManager);
    }

    // no need to keep it past this point; pool is thread-safe
    lock.unlock();

    if (_apmCallback) {
        // TODO: Remove this conditional when TIG-1396 is resolved.
        auto connection = pool->acquire();
        return shouldPrewarm ? _preWarm(std::move(connection)) : std::move(connection);
    } else {
        auto opEntry = pool->try_acquire();
        if (!opEntry) {
            // TODO: better error handling
            throw InvalidConfigurationException("Failed to acquire an entry from the client pool.");
        }
        return shouldPrewarm ? _preWarm(std::move(*opEntry)) : std::move(*opEntry);
    }
}

mongocxx::pool::entry genny::v1::PoolManager::_preWarm(mongocxx::pool::entry connection) {
    auto ping = bsoncxx::builder::stream::document{} << "ping" << 1 << bsoncxx::builder::stream::finalize;
    connection->database("admin").run_command(ping.view());
    return std::move(connection);
}

std::unordered_map<std::string, size_t> genny::v1::PoolManager::instanceCount() {
    std::lock_guard<std::mutex> getLock{this->_poolsLock};

    auto out = std::unordered_map<std::string, size_t>();
    for (auto&& [k, v] : this->_pools) {
        out[k] = v.second.size();
    }
    return out;
}
