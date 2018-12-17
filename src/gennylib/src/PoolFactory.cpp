#include <gennylib/PoolFactory.hpp>

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
        const auto protocolRegex = std::regex("^(mongodb://|mongodb+srv://)?(([^:@]*):([^@]*)@)?");
        const auto hostRegex = std::regex("^,?([^:,/]+(:[0-9]+)?)");
        const auto dbRegex = std::regex("^/([^?]*)\\??");
        const auto queryRegex = std::regex("^&?([^=&]*)=([^&]*)");

        std::cmatch matches;
        auto i = 0;

        // Extract the protocol, and optionally the username and the password
        std::regex_search(uri.substr(i).data(), matches, protocolRegex);
        if (matches.length(1)) {
            accessOptions["Protocol"] = matches[1];
        } else {
            accessOptions["Protocol"] = "mongodb://";
        }
        if (matches.length(3))
            accessOptions["Username"] = matches[3];
        if (matches.length(4))
            accessOptions["Password"] = matches[4];
        i += matches.length();

        // Extract each host specified in the uri
        while (std::regex_search(uri.substr(i).data(), matches, hostRegex)) {
            hosts.insert(matches[1]);
            i += matches.length();
        }

        // Extract the db name and optionally the query string prefix
        std::regex_search(uri.substr(i).data(), matches, dbRegex);
        accessOptions["Database"] = matches[1];
        i += matches.length();

        // Extract each query parameter
        // Note that the official syntax of query strings is poorly defined, keys without values may
        // be valid but not supported here.
        while (std::regex_search(uri.substr(i).data(), matches, queryRegex)) {
            auto key = matches[1];
            auto value = matches[2];
            queryOptions[key] = value;
            i += matches.length();
        }
    }

    auto makeUri() const {
        std::ostringstream ss;
        size_t i;

        ss << accessOptions.at("Protocol");
        if (!accessOptions.at("Username").empty()) {
            ss << accessOptions.at("Username") << ':' << accessOptions.at("Password") << '@';
        }

        i = 0;
        for (auto& host : hosts) {
            if (i++ > 0) {
                ss << ',';
            }
            ss << host;
        }

        auto dbName = accessOptions.at("Database");
        if (!dbName.empty() || !queryOptions.empty()) {
            ss << '/' << accessOptions.at("Database");
        }

        if (!queryOptions.empty()) {
            ss << '?';
        }

        i = 0;
        for (auto && [ key, value ] : queryOptions) {
            if (i++ > 0) {
                ss << '&';
            }
            ss << key << '=' << value;
        }

        return ss.str();
    }

    std::set<std::string> hosts;
    std::map<std::string, std::string> queryOptions;
    std::map<std::string, std::string> accessOptions = {
        {"Protocol", ""},
        {"Username", ""},
        {"Password", ""},
        {"Database", ""},
        {"AllowInvalidCertificates", ""},
        {"CAFile", ""},
    };
};

PoolFactory::PoolFactory(std::string_view rawUri) : _config(std::make_unique<Config>(rawUri)) {}
PoolFactory::~PoolFactory() {}

std::string PoolFactory::makeUri() const {
    return _config->makeUri();
}

mongocxx::options::pool PoolFactory::makeOptions() const {
    mongocxx::options::ssl sslOptions;
    if (_config->accessOptions.at("AllowInvalidCertificates") == "true") {
        sslOptions.allow_invalid_certificates(true);
    }

    // Just doing CAFile for now, it's reasonably trivial to add other options
    // Note that this is entering as a BSON string view, so you cannot delete the config object
    auto& caFile = _config->accessOptions.at("CAFile");
    if (!caFile.empty()) {
        std::cout << caFile << std::endl;
        sslOptions.ca_file(caFile);
    }

    mongocxx::options::client clientOptions;
    clientOptions.ssl_opts(sslOptions);
    return clientOptions;
}
std::unique_ptr<mongocxx::pool> PoolFactory::makePool() const {
    auto uriStr = makeUri();
    BOOST_LOG_TRIVIAL(info) << "Constructing pool with MongoURI '" << uriStr << "'";

    auto uri = mongocxx::uri{uriStr};

    auto poolOptions = mongocxx::options::pool{};
    auto sslIt = _config->queryOptions.find("ssl");
    if (sslIt != _config->queryOptions.end() && sslIt->second == "true") {
        auto poolOptions = makeOptions();
    }

    return std::make_unique<mongocxx::pool>(uri, poolOptions);
}

void PoolFactory::setStringOption(const std::string& option, std::string value) {
    // If the value is in the accessOptions set, set it
    auto it = _config->accessOptions.find(option);
    if (it != _config->accessOptions.end()) {
        it->second = value;
        return;
    }

    // Treat the value as a normal query parameter
    _config->queryOptions[option] = value;
}

void PoolFactory::setIntOption(const std::string& option, int32_t value) {
    auto valueStr = std::to_string(value);
    setStringOption(option, valueStr);
}

void PoolFactory::setFlag(const std::string& option, bool value) {
    auto valueStr = value ? "true" : "false";
    setStringOption(option, valueStr);
}

}  // namespace genny
