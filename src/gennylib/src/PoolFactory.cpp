#include <gennylib/PoolFactory.hpp>

#include <mongoc/mongoc.h>

#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include <boost/log/trivial.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/InvalidConfigurationException.hpp>

namespace genny {

struct PoolFactory::Config {
    Config(std::string_view uri) {
        const auto protocolRegex = std::regex("^(mongodb://)?(([^:@]*):([^@]*)@)?");
        const auto hostRegex = std::regex("^,?([^:,/]+(:[0-9]+)?)");
        const auto dbRegex = std::regex("^/?([^?]*)\\?");
        const auto queryRegex = std::regex("^&?([^=&]*)=([^&]*)");

        std::cmatch matches;
        auto i = 0;

        std::regex_search(uri.substr(i).data(), matches, protocolRegex);
        if (matches.length(1)) {
            extraOptions["Protocol"] = matches[1];
        } else {
            extraOptions["Protocol"] = "mongodb://";
        }
        if (matches.length(3))
            extraOptions["Username"] = matches[3];
        if (matches.length(4))
            extraOptions["Password"] = matches[4];
        i += matches.length();

        while (std::regex_search(uri.substr(i).data(), matches, hostRegex)) {
            hosts.insert(matches[1]);
            i += matches.length();
        }

        std::regex_search(uri.substr(i).data(), matches, dbRegex);
        extraOptions["Database"] = matches[1];
        i += matches.length();

        while (std::regex_search(uri.substr(i).data(), matches, queryRegex)) {
            auto key = matches[1];
            auto value = matches[2];
            options[key] = value;
            i += matches.length();
        }
    }

    auto makeUri() const {
        std::ostringstream ss;
        size_t i;

        ss << extraOptions.at("Protocol");
        if (!extraOptions.at("Username").empty()) {
            ss << extraOptions.at("Username") << ':' << extraOptions.at("Password") << '@';
        }

        i = 0;
        for (auto& host : hosts) {
            if (i++ > 0) {
                ss << ',';
            }
            ss << host;
        }

        auto dbName = extraOptions.at("Database");
        if (!dbName.empty() || !options.empty()) {
            ss << '/' << extraOptions.at("Database");
        }

        if (!options.empty()) {
            ss << '?';
        }

        i = 0;
        for (auto && [ key, value ] : options) {
            if (i++ > 0) {
                ss << '&';
            }
            ss << key << '=' << value;
        }

        return ss.str();
    }

    std::set<std::string> hosts;
    std::map<std::string, std::string> options;
    std::map<std::string, std::string> extraOptions = {
        {"Protocol", ""}, {"Username", ""}, {"Password", ""}, {"Database", ""},
    };
    mongocxx::options::pool poolOptions;
};

PoolFactory::PoolFactory(std::string_view rawUri) : _config(std::make_unique<Config>(rawUri)) {}
PoolFactory::~PoolFactory() {}

std::string PoolFactory::makeUri() const {
    return _config->makeUri();
}

std::unique_ptr<mongocxx::pool> PoolFactory::makePool() const {
    auto uriStr = makeUri();
    BOOST_LOG_TRIVIAL(info) << "Constructing pool with MongoURI '" << uriStr << "'";

    auto uri = mongocxx::uri{uriStr};
    return std::make_unique<mongocxx::pool>(uri, _config->poolOptions);
}

void PoolFactory::setStringOption(const std::string& option, std::string value) {
    // If the value is in the extraOption set, set it
    auto it = _config->extraOptions.find(option);
    if (it != _config->extraOptions.end()) {
        it->second = value;
        return;
    }

    // Treat the value as a normal query parameter
    _config->options[option] = value;
}

void PoolFactory::setIntOption(const std::string& option, int32_t value) {
    auto valueStr = std::to_string(value);
    setStringOption(option, valueStr);
}

void PoolFactory::setFlag(const std::string& option, bool value) {
    auto valueStr = value ? "true" : "false";
    setStringOption(option, valueStr);
}

void PoolFactory::configureSsl(mongocxx::options::ssl options) {
    setFlag("ssl", true);

    auto clientOpts = _config->poolOptions.client_opts();
    _config->poolOptions = clientOpts.ssl_opts(options);
}

}  // namespace genny
