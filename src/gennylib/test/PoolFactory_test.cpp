// Copyright 2018 MongoDB Inc.
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

#include "test.h"

#include <iostream>

#include <gennylib/PoolFactory.hpp>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

namespace Catchers = Catch::Matchers;

using OptionType = genny::PoolFactory::OptionType;

TEST_CASE("PoolFactory behavior") {
    mongocxx::instance::current();

    // Testing out core features of the PoolFactory
    SECTION("Make a few trivial localhost pools") {
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

    SECTION("Make a pool with the bare minimum uri") {
        constexpr auto kSourceUri = "127.0.0.1";

        auto factory = genny::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&]() { return std::string{"mongodb://"} + kSourceUri; };
        REQUIRE(factoryUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);
    }

    SECTION("Replace the replset and the db") {
        constexpr auto kSourceUri = "mongodb://127.0.0.1/bigdata?replicaSet=badChoices";

        const std::string kBaseString = "mongodb://127.0.0.1/";

        auto factory = genny::PoolFactory(kSourceUri);

        SECTION("Validate the original URI") {
            auto factoryUri = factory.makeUri();
            auto expectedUri = [&]() { return kBaseString + "bigdata?replicaSet=badChoices"; };
            REQUIRE(factoryUri == expectedUri());

            auto pool = factory.makePool();
            REQUIRE(pool);
        }

        SECTION("Modify the URI and check that it works") {
            auto expectedUri = [&]() { return kBaseString + "webscale?replicaSet=threeNode"; };
            factory.setOption(OptionType::kQueryOption, "replicaSet", "threeNode");
            factory.setOption(OptionType::kAccessOption, "Database", "webscale");

            auto factoryUri = factory.makeUri();
            REQUIRE(factoryUri == expectedUri());

            auto pool = factory.makePool();
            REQUIRE(pool);
        }
    }

    SECTION("Try various set() commands with odd cases") {
        const std::string kBaseString = "mongodb://127.0.0.1/";
        constexpr auto kOriginalDatabase = "admin";

        SECTION("Use the wrong case for 'Database' option") {
            auto sourceUri = [&]() { return kBaseString + kOriginalDatabase; };
            auto factory = genny::PoolFactory(sourceUri());

            auto expectedUri = [&]() { return sourceUri() + "?database=test"; };
            factory.setOption(OptionType::kQueryOption, "database", "test");

            auto factoryUri = factory.makeUri();
            REQUIRE(factoryUri == expectedUri());

            auto badSet = [&]() {
                factory.setOption(OptionType::kAccessOption, "database", "test");
            };
            REQUIRE_THROWS(badSet());
        }

        // Funny enough, going through MongoURI means we convert to strings.
        // So we can set access options like 'Database' through functions we would
        // not normally consider for traditional string flags
        SECTION("Set the 'Database' option in odd ways") {
            auto sourceUri = [&]() { return kBaseString + kOriginalDatabase; };
            auto factory = genny::PoolFactory(sourceUri());


            SECTION("Use the flag option") {
                auto expectedUri = [&]() { return kBaseString + "true"; };
                factory.setFlag(OptionType::kAccessOption, "Database");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
            }

            SECTION("Use the string option to reset the Database") {
                auto expectedUri = [&]() { return kBaseString + "true"; };
                factory.setFlag(OptionType::kAccessOption, "Database", "true");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
            }

            SECTION("Use the flag option to flip the Database") {
                auto expectedUri = [&]() { return kBaseString + "false"; };
                factory.setFlag(OptionType::kAccessOption, "Database", false);

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
            }
        }

        SECTION("Overwrite the replSet option in a variety of ways") {
            auto sourceUri = [&]() { return kBaseString + "?replSet=red"; };
            auto factory = genny::PoolFactory(sourceUri());

            SECTION("Overwrite with a normal string") {
                auto expectedUri = [&]() { return kBaseString + "?replSet=blue"; };
                factory.setOption(OptionType::kQueryOption, "replSet", "blue");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
            }

            SECTION("Overwrite with an empty string") {
                // An empty string is still a valid option, even if not a valid replset
                auto expectedUri = [&]() { return kBaseString + "?replSet="; };
                factory.setOption(OptionType::kQueryOption, "replSet", "");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
            }
        }
    }

    // Moving on to actual pool cases
    SECTION("Make a pool with a severely limited max size") {
        const std::string kSourceUri = "mongodb://127.0.0.1";
        constexpr int32_t kMaxPoolSize = 2;

        auto factory = genny::PoolFactory(kSourceUri);

        auto expectedUri = [&]() { return kSourceUri + "/?maxPoolSize=2"; };
        factory.setOptionFromInt(OptionType::kQueryOption, "maxPoolSize", kMaxPoolSize);

        auto factoryUri = factory.makeUri();
        REQUIRE(factoryUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);

        std::vector<std::unique_ptr<mongocxx::pool::entry>> clients;
        for (int i = 0; i < kMaxPoolSize; ++i) {
            auto client = pool->try_acquire();
            auto gotClient = static_cast<bool>(client);
            REQUIRE(gotClient);

            clients.emplace_back(std::make_unique<mongocxx::pool::entry>(std::move(*client)));
        }

        // We should be full up now
        auto getOneMore = [&]() { return !static_cast<bool>(pool->try_acquire()); };
        REQUIRE(getOneMore());
    }

    SECTION("Make a pool with ssl enabled and auth params") {
        const std::string kProtocol = "mongodb://";
        const std::string kHost = "127.0.0.1";
        constexpr auto kCAFile = "some-random-ca.pem";

        auto sourceUrl = [&]() { return kProtocol + kHost; };
        auto factory = genny::PoolFactory(sourceUrl());

        auto expectedUri = [&]() { return kProtocol + "boss:pass@" + kHost + "/admin?ssl=true"; };
        factory.setOptions(OptionType::kAccessOption,
                           {{"Username", "boss"}, {"Password", "pass"}, {"Database", "admin"}});

        factory.setFlag(OptionType::kQueryOption, "ssl");
        factory.setFlag(OptionType::kAccessOption, "AllowInvalidCertificates");
        factory.setOption(OptionType::kAccessOption, "CAFile", kCAFile);

        auto factoryUri = factory.makeUri();
        REQUIRE(factoryUri == expectedUri());

        auto factoryOpts = factory.makeOptions();

        auto sslOpts = *factoryOpts.client_opts().ssl_opts();
        auto allowInvalid = *sslOpts.allow_invalid_certificates();
        REQUIRE(allowInvalid);

        std::string caFile = sslOpts.ca_file()->terminated().data();
        REQUIRE(kCAFile == caFile);

        auto pool = factory.makePool();
        REQUIRE(pool);
    }
}
