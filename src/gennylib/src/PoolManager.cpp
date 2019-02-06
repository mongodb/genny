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

#include <gennylib/PoolManager.hpp>
#include <gennylib/context.hpp>
#include <gennylib/v1/PoolFactory.hpp>

namespace {

auto createPool(const std::string& mongoUri,
                genny::PoolManager::OnCommandStartCallback& apmCallback,
                genny::WorkloadContext& context) {
    auto poolFactory = genny::v1::PoolFactory(mongoUri, apmCallback);

    auto queryOpts =
        context.get_noinherit<std::map<std::string, std::string>, false>("Pool", "QueryOptions");
    if (queryOpts) {
        poolFactory.setOptions(genny::v1::PoolFactory::kQueryOption, *queryOpts);
    }

    auto accessOpts =
        context.get_noinherit<std::map<std::string, std::string>, false>("Pool", "AccessOptions");
    if (accessOpts) {
        poolFactory.setOptions(genny::v1::PoolFactory::kAccessOption, *accessOpts);
    }

    return poolFactory.makePool();
}

}  // namespace


mongocxx::pool::entry genny::PoolManager::client(const std::string& name,
                                                 unsigned long instance,
                                                 genny::WorkloadContext& context) {
    // Only one thread can access pools.operator[] at a time...
    this->_poolsGet.lock();
    LockAndPools& lap = this->_pools[name];
    // ...but no need to keep the lock open past this.
    // Two threads trying access client("foo",0) at the same
    // time will subsequently block on the unique_lock.
    this->_poolsGet.unlock();

    // only one thread can access the vector of pools at once
    std::unique_lock lock{lap.first};

    Pools& pools = lap.second;

    while (pools.empty() || pools.size() - 1 < instance) {
        pools.push_back(createPool(this->_mongoUri, this->_apmCallback, context));
    }
    // .at does range-checking to help catch bugs with above logic :)
    auto& pool = pools.at(instance);

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
