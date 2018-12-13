#include "test.h"

#include <gennylib/PoolFactory.hpp>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

namespace Catchers = Catch::Matchers;

TEST_CASE("PoolFactory behavior") {
    mongocxx::instance::current();

    SECTION("Make a few trivial localhost pools"){
        constexpr auto kSourceUri = "mongodb://127.0.0.1:27017";

        auto factory = genny::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        REQUIRE(factoryUri == kSourceUri);

        auto pool = factory.makePool();
        REQUIRE(pool);

        // We should be able to get more from the same factory
        auto extraPool = factory.makePool();
        REQUIRE(extraPool);
    }

    SECTION("Make a pool with the bare minimum uri"){
        constexpr auto kSourceUri = "127.0.0.1";

        auto factory = genny::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&](){
            std::string uri{"mongodb://"};
            uri += kSourceUri;
            return uri;
        };
        REQUIRE(factoryUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);
    }

    SECTION("Make a pool with a severely limited max size"){
        constexpr auto kSourceUri = "mongodb://127.0.0.1";
        constexpr auto kQueryString = "/?maxPoolSize=2";
        constexpr auto kOptionKey = "maxPoolSize";
        constexpr int32_t kMaxPoolSize = 2;

        auto factory = genny::PoolFactory(kSourceUri);
        factory.setIntOption(kOptionKey, kMaxPoolSize);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&](){
            std::string uri{kSourceUri};
            uri += kQueryString;
            return uri;
        };
        REQUIRE(factoryUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);

        std::vector<std::unique_ptr<mongocxx::pool::entry>> clients;
        for(int i = 0; i < kMaxPoolSize; ++i){
            auto client = pool->try_acquire();
            auto gotClient = static_cast<bool>(client);
            REQUIRE(gotClient);

            clients.emplace_back(std::make_unique<mongocxx::pool::entry>(std::move(*client)));
        }

        // We should be full up now
        auto getOneMore = [&]() { return !static_cast<bool>(pool->try_acquire()); };
        REQUIRE(getOneMore());
    }

    SECTION("Make a pool with ssl enabled and auth params"){
        constexpr auto kSourceUri = "mongodb://127.0.0.1";
        constexpr auto kAuthString = "boss:pass@";
        constexpr auto kHostString = "127.0.0.1";
        constexpr auto kQueryString = "/admin?ssl=true";

        constexpr auto kUsername = "boss";
        constexpr auto kPassword = "pass";
        constexpr auto kDatabase = "admin";

        auto factory = genny::PoolFactory(kSourceUri);

        factory.setStringOption("Username", kUsername);
        factory.setStringOption("Password", kPassword);
        factory.setStringOption("Database", kDatabase);

        // This won't actually work for real, but it does test the interface
        mongocxx::options::ssl sslOpts;
        sslOpts.allow_invalid_certificates(true);
        factory.configureSsl(sslOpts);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&](){
            std::string uri{"mongodb://"};
            uri += kAuthString;
            uri += kHostString;
            uri += kQueryString;
            return uri;
        };
        REQUIRE(factoryUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);
    }
}
