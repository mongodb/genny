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
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include <mongocxx/pool.hpp>

namespace genny {

class PoolManager {
public:
    using CallMeMaybe = std::optional<std::function<void(const mongocxx::events::command_started_event&)>>;

    PoolManager(const std::string& mongoUri,
                CallMeMaybe callback)
        : _mongoUri{mongoUri},
          _apmCallback{callback}
        {}

    mongocxx::pool::entry client(const std::string& name,
                                 int instance,
                                 class WorkloadContext& context);

private:
    using Pools = std::vector<std::unique_ptr<mongocxx::pool>>;
    using LockAndPools = std::pair<std::shared_mutex, Pools>;
    std::unordered_map<std::string, LockAndPools> _pools;
    std::shared_mutex _poolsGet;
    CallMeMaybe _apmCallback;
    std::string _mongoUri;
};

}  // namespace genny

#endif  // HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
