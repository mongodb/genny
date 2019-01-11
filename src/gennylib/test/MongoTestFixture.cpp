#include "MongoTestFixture.hpp"

#include <cstdint>
#include <string_view>

#include <mongocxx/uri.hpp>

#include <log.hh>

namespace genny::testing {

mongocxx::uri MongoTestFixture::connectionUri() {
    const char* connChar = getenv("MONGO_CONNECTION_STRING");

    if (connChar != nullptr) {
        return mongocxx::uri(connChar);
    }

    auto& connStr = mongocxx::uri::k_default_uri;
    BOOST_LOG_TRIVIAL(info) << "MONGO_CONNECTION_STRING not set, using default value: " << connStr;
    return mongocxx::uri(connStr);
};

void MongoTestFixture::dropAllDatabases() {
    for (auto&& dbDoc : client.list_databases()) {
        const auto dbName = dbDoc["name"].get_utf8().value;
        const auto dbNameString = std::string(dbName);
        if (dbNameString != "admin" && dbNameString != "config" && dbNameString != "local") {
            client.database(dbName).drop();
        }
    }
}

}  // namespace genny::testing
