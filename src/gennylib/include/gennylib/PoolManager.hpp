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

namespace genny {

class PoolManager {
public:
    using OnCommandStartCallback =
        std::function<void(const mongocxx::events::command_started_event&)>;

    PoolManager(std::string mongoUri, OnCommandStartCallback callback)
        : _mongoUri{std::move(mongoUri)}, _apmCallback{std::move(callback)} {}

    mongocxx::pool::entry client(const std::string& name,
                                 size_t instance,
                                 class WorkloadContext& context);

private:
    std::string _mongoUri;
    OnCommandStartCallback _apmCallback;

    using Pools = std::unordered_map<size_t,std::unique_ptr<mongocxx::pool>>;
    // pair each map ↑ with a mutex for adding new pools
    using LockAndPools = std::pair<std::mutex, Pools>;

    std::unordered_map<std::string, LockAndPools> _pools;
    std::mutex _poolsGet;
};

}  // namespace genny

#endif  // HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
