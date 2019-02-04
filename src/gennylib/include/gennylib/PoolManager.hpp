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

#include <mongocxx/pool.hpp>

namespace genny {

class PoolManager {
public:
    PoolManager(const std::string& mongoUri,
                std::function<void(const mongocxx::events::command_started_event&)> callback)
        : _mongoUri{mongoUri},
          _apmCallback{callback},
          // TODO: Remove hasApmOpts when TIG-1396 is resolved.
          _hasApmOpts{bool{callback}} {}

    mongocxx::pool::entry client(const std::string& name,
                                 int instance,
                                 class WorkloadContext& context);

private:
    using Pools = std::vector<std::unique_ptr<mongocxx::pool>>;
    using LockAndPools = std::pair<std::shared_mutex, Pools>;
    std::unordered_map<std::string, LockAndPools> _pools;
    std::shared_mutex _poolsGet;
    // TODO: is cast to bool right here?
    // TODO: should this be a ref? who owns the callback?
    std::function<void(const mongocxx::events::command_started_event&)> _apmCallback;
    std::string _mongoUri;

    // A flag representing the presence of application performance monitoring options used for
    // testing. This can be removed once TIG-1396 is resolved.
    bool _hasApmOpts;
};

}  // namespace genny

#endif  // HEADER_088A462A_CF7B_4114_841E_C19AA8D29774_INCLUDED
