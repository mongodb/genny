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

#ifndef HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B
#define HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include <gennylib/PoolManager.hpp>
#include <mongocxx/pool.hpp>

namespace genny::v1 {

/**
 * A pool factory takes in a MongoURI, modifies its components, and makes a pool from it
 *
 * This class allows for programically modifying all non-host components of the MongoURI.
 * Any query parameter can be set via `setStringOption()`, `setIntOption()`, or `setFlag()`.
 * It also allows for setting the protocol, username, password, and database via the options
 * "Protocol", "Username", "Password", and "Database" in the same manner as query parameters would
 * be set. Lastly, it allows for programatically setting up the ssl options for the connection pool
 * via `configureSsl()`.
 */
class PoolFactory {
public:
    enum OptionType {
        kQueryOption = 0,  // Default to query option
        kAccessOption,
    };

public:
    PoolFactory(std::string_view uri, PoolManager::CallMeMaybe callback);
    ~PoolFactory();

    // Both `makeUri()` and `makeOptions()` are used internally. They are publicly exposed to
    // facilitate testing.
    std::string makeUri() const;
    mongocxx::options::pool makeOptions() const;

    std::unique_ptr<mongocxx::pool> makePool() const;

    /**
     * Options of note:
     *  minPoolSize
     *  maxPoolSize
     *  connectTimeoutMS
     *  socketTimeoutMS
     */
    void setOption(OptionType type, const std::string& option, std::string value);

    template <typename ContainerT = std::map<std::string, std::string>>
    void setOptions(OptionType type, ContainerT list) {
        for (const auto& [key, value] : list) {
            setOption(type, key, value);
        }
    }

    void setOptionFromInt(OptionType type, const std::string& option, int32_t value);
    void setFlag(OptionType type, const std::string& option, bool value = true);

    std::optional<std::string_view> getOption(OptionType type, const std::string& option) const;

private:
    struct Config;
    std::unique_ptr<Config> _config;
    PoolManager::CallMeMaybe _apmCallback;
};

}  // namespace genny::v1

#endif  // HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B
