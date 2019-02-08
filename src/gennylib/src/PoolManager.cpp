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

#include <gennylib/v1/PoolFactory.hpp>
#include <gennylib/v1/PoolManager.hpp>

namespace genny::v1 {
namespace {

auto createPool(const std::string& mongoUri,
                const std::string& name,
                PoolManager::OnCommandStartCallback& apmCallback,
                const ConfigNode& context) {
    auto poolFactory = PoolFactory(mongoUri, apmCallback);

    auto queryOpts = context.get_noinherit<std::map<std::string, std::string>, false>(
        "Clients", name, "QueryOptions");
    if (queryOpts) {
        poolFactory.setOptions(PoolFactory::kQueryOption, *queryOpts);
    }

    auto accessOpts = context.get_noinherit<std::map<std::string, std::string>, false>(
        "Clients", name, "AccessOptions");
    if (accessOpts) {
        poolFactory.setOptions(genny::v1::PoolFactory::kAccessOption, *accessOpts);
    }

    return poolFactory.makePool();
}

}  // namespace

}  // namespace genny::v1


mongocxx::pool::entry genny::v1::PoolManager::client(const std::string& name,
                                                     size_t instance,
                                                     const genny::v1::ConfigNode& context) {
    // Only one thread can access pools.operator[] at a time...
    std::unique_lock<std::mutex> getLock{this->_poolsGet};
    LockAndPools& lap = this->_pools[name];
    // ...but no need to keep the lock open past this.
    // Two threads trying access client("foo",0) at the same
    // time will subsequently block on the unique_lock.
    getLock.unlock();

    // only one thread can access the map of pools at once
    std::unique_lock<std::mutex> lock{lap.first};

    Pools& pools = lap.second;

    auto& pool = pools[instance];
    if (pool == nullptr) {
        pool = createPool(this->_mongoUri, name, this->_apmCallback, context);
    }

    // no need to keep it past this point; pool is thread-safe
    lock.unlock();

    if (_apmCallback) {
        // TODO: Remove this conditional when TIG-1396 is resolved.
        return pool->acquire();
    }
    auto entry = pool->try_acquire();
    if (!entry) {
        // TODO: better error handling
        throw InvalidConfigurationException("Failed to acquire an entry from the client pool.");
    }
    return std::move(*entry);
}

std::unordered_map<std::string, size_t> genny::v1::PoolManager::instanceCount() {
    auto out = std::unordered_map<std::string, size_t>();
    for (auto&& [k, v] : this->_pools) {
        out[k] = v.second.size();
    }
    return out;
}
