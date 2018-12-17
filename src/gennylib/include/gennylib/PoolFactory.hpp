#ifndef HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B
#define HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <mongocxx/pool.hpp>

namespace genny {

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
    PoolFactory(std::string_view uri);
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
    void setOption(OptionType type, const std::string & option, std::string value);

    template <typename ContainerT = std::map<std::string, std::string>>
    void setOptions(OptionType type, ContainerT list) {
        for (const auto & [ key, value ] : list) {
            setOption(type, key, value);
        }
    }

    void setOptionFromInt(OptionType type, const std::string & option, int32_t value);
    void setFlag(OptionType type, const std::string & option, bool value = true);

    std::optional<std::string_view> getOption(OptionType type, const std::string& option) const;

private:
    struct Config;
    std::unique_ptr<Config> _config;
};

} // namespace genny

#endif // HEADER_3BB17688_900D_4AFB_B736_C9EC8DA9E33B
