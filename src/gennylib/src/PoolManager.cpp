
#include <gennylib/PoolManager.hpp>
// TODO: move PoolFactory to v1
#include <gennylib/context.hpp>
#include <gennylib/v1/PoolFactory.hpp>

namespace {

auto createPool(std::string mongoUri,
                std::string name,
                genny::PoolManager::CallMeMaybe& apmCallback,
                genny::WorkloadContext& context) {
    // TODO: make this optional and default to mongodb://localhost:27017
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
                                                 int instance,
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
        pools.push_back(createPool(this->_mongoUri, name, this->_apmCallback, context));
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
