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

#include <iostream>

#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

#include <gennylib/v1/PoolFactory.hpp>
#include <gennylib/v1/PoolManager.hpp>

#include <metrics/metrics.hpp>

#include <testlib/ActorHelper.hpp>
#include <testlib/helpers.hpp>


namespace Catchers = Catch::Matchers;

using OptionType = genny::v1::PoolFactory::OptionType;

TEST_CASE("PoolFactory behavior") {
    mongocxx::instance::current();

    // Testing out core features of the PoolFactory
    SECTION("Make a few trivial localhost pools") {
        constexpr auto kSourceUri = "mongodb://127.0.0.1:27017";

        auto factory = genny::v1::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&]() { return kSourceUri + std::string{"/?appName=Genny"}; };
        REQUIRE(factoryUri == expectedUri());
        auto redactedUri = factory.makeRedactedUri();
        REQUIRE(redactedUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);

        // We should be able to get more from the same factory
        auto extraPool = factory.makePool();
        REQUIRE(extraPool);
    }

    SECTION("Make a pool with the bare minimum uri") {
        constexpr auto kSourceUri = "127.0.0.1";

        auto factory = genny::v1::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&]() {
            return std::string{"mongodb://"} + kSourceUri + std::string{"/?appName=Genny"};
        };
        REQUIRE(factoryUri == expectedUri());
        auto redactedUri = factory.makeRedactedUri();
        REQUIRE(redactedUri == expectedUri());

        auto pool = factory.makePool();
        REQUIRE(pool);
    }

    SECTION("Replace the replset and the db") {
        constexpr auto kSourceUri = "mongodb://127.0.0.1/bigdata?replicaSet=badChoices";

        const std::string kBaseString = "mongodb://127.0.0.1/";

        auto factory = genny::v1::PoolFactory(kSourceUri);

        SECTION("Validate the original URI") {
            auto factoryUri = factory.makeUri();
            auto expectedUri = [&]() {
                return kBaseString + "bigdata?appName=Genny&replicaSet=badChoices";
            };
            REQUIRE(factoryUri == expectedUri());
            auto redactedUri = factory.makeRedactedUri();
            REQUIRE(redactedUri == expectedUri());

            auto pool = factory.makePool();
            REQUIRE(pool);
        }

        SECTION("Modify the URI and check that it works") {
            auto expectedUri = [&]() {
                return kBaseString + "webscale?appName=Genny&replicaSet=threeNode";
            };
            factory.setOption(OptionType::kQueryOption, "replicaSet", "threeNode");
            factory.setOption(OptionType::kAccessOption, "Database", "webscale");

            auto factoryUri = factory.makeUri();
            REQUIRE(factoryUri == expectedUri());
            auto redactedUri = factory.makeRedactedUri();
            REQUIRE(redactedUri == expectedUri());

            auto pool = factory.makePool();
            REQUIRE(pool);
        }
    }

    SECTION("Try various set() commands with odd cases") {
        const std::string kBaseString = "mongodb://127.0.0.1/";
        constexpr auto kOriginalDatabase = "admin";

        SECTION("Use the wrong case for 'Database' option") {
            auto sourceUri = [&]() { return kBaseString + kOriginalDatabase; };
            auto factory = genny::v1::PoolFactory(sourceUri());

            auto expectedUri = [&]() { return sourceUri() + "?appName=Genny&database=test"; };
            factory.setOption(OptionType::kQueryOption, "database", "test");

            auto factoryUri = factory.makeUri();
            REQUIRE(factoryUri == expectedUri());
            auto redactedUri = factory.makeRedactedUri();
            REQUIRE(redactedUri == expectedUri());

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
            auto factory = genny::v1::PoolFactory(sourceUri());


            SECTION("Use the flag option") {
                auto expectedUri = [&]() { return kBaseString + "true?appName=Genny"; };
                factory.setFlag(OptionType::kAccessOption, "Database");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
                auto redactedUri = factory.makeRedactedUri();
                REQUIRE(redactedUri == expectedUri());
            }

            SECTION("Use the string option to reset the Database") {
                auto expectedUri = [&]() { return kBaseString + "true?appName=Genny"; };
                factory.setFlag(OptionType::kAccessOption, "Database", "true");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
                auto redactedUri = factory.makeRedactedUri();
                REQUIRE(redactedUri == expectedUri());
            }

            SECTION("Use the flag option to flip the Database") {
                auto expectedUri = [&]() { return kBaseString + "false?appName=Genny"; };
                factory.setFlag(OptionType::kAccessOption, "Database", false);

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
                auto redactedUri = factory.makeRedactedUri();
                REQUIRE(redactedUri == expectedUri());
            }
        }

        SECTION("Overwrite the replSet option in a variety of ways") {
            auto sourceUri = [&]() { return kBaseString + "?appName=Genny&replSet=red"; };
            auto factory = genny::v1::PoolFactory(sourceUri());

            SECTION("Overwrite with a normal string") {
                auto expectedUri = [&]() { return kBaseString + "?appName=Genny&replSet=blue"; };
                factory.setOption(OptionType::kQueryOption, "replSet", "blue");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
                auto redactedUri = factory.makeRedactedUri();
                REQUIRE(redactedUri == expectedUri());
            }

            SECTION("Overwrite with an empty string") {
                // An empty string is still a valid option, even if not a valid replset
                auto expectedUri = [&]() { return kBaseString + "?appName=Genny&replSet="; };
                factory.setOption(OptionType::kQueryOption, "replSet", "");

                auto factoryUri = factory.makeUri();
                REQUIRE(factoryUri == expectedUri());
                auto redactedUri = factory.makeRedactedUri();
                REQUIRE(redactedUri == expectedUri());
            }
        }
    }

    // Moving on to actual pool cases
    SECTION("Make a pool with a severely limited max size") {
        const std::string kSourceUri = "mongodb://127.0.0.1";
        constexpr int32_t kMaxPoolSize = 2;

        auto factory = genny::v1::PoolFactory(kSourceUri);

        auto expectedUri = [&]() { return kSourceUri + "/?appName=Genny&maxPoolSize=2"; };
        factory.setOptionFromInt(OptionType::kQueryOption, "maxPoolSize", kMaxPoolSize);

        auto factoryUri = factory.makeUri();
        REQUIRE(factoryUri == expectedUri());
        auto redactedUri = factory.makeRedactedUri();
        REQUIRE(redactedUri == expectedUri());

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

    SECTION("Make a pool with tls enabled and auth params") {
        const std::string kProtocol = "mongodb://";
        const std::string kHost = "127.0.0.1";
        constexpr auto kCAFile = "some-random-ca.pem";

        auto sourceUrl = [&]() { return kProtocol + kHost; };
        auto factory = genny::v1::PoolFactory(sourceUrl());

        auto expectedUri = [&]() {
            return kProtocol + "boss:pass@" + kHost + "/admin?appName=Genny&tls=true";
        };
        factory.setOptions(OptionType::kAccessOption,
                           {{"Username", "boss"}, {"Password", "pass"}, {"Database", "admin"}});

        factory.setFlag(OptionType::kQueryOption, "tls");
        factory.setFlag(OptionType::kAccessOption, "AllowInvalidCertificates");
        factory.setOption(OptionType::kAccessOption, "CAFile", kCAFile);

        auto factoryUri = factory.makeUri();
        REQUIRE(factoryUri == expectedUri());

        auto expectedRedactedUri = [&]() {
            return kProtocol + "boss:[REDACTED]@" + kHost + "/admin?appName=Genny&tls=true";
        };
        auto redactedUri = factory.makeRedactedUri();
        REQUIRE(redactedUri == expectedRedactedUri());

        auto factoryOpts = factory.makeOptions();

        auto tlsOpts = *factoryOpts.client_opts().tls_opts();
        auto allowInvalid = *tlsOpts.allow_invalid_certificates();
        REQUIRE(allowInvalid);

        std::string caFile = tlsOpts.ca_file()->terminated().data();
        REQUIRE(kCAFile == caFile);

        auto pool = factory.makePool();
        REQUIRE(pool);

        // We should be able to change the value of an option.
        // Also, PoolFactory should work with empty password.
        auto expectedPassEmptyUri = [&]() {
            return kProtocol + "boss:@" + kHost + "/admin?appName=Genny&tls=true";
        };
        factory.setOption(OptionType::kAccessOption, "Password", "");
        auto passEmptyUri = factory.makeUri();
        REQUIRE(passEmptyUri == expectedPassEmptyUri());

        // Should hide the fact that password is empty
        auto expectedRedactedPassEmptyUri = [&]() {
            return kProtocol + "boss:[REDACTED]@" + kHost + "/admin?appName=Genny&tls=true";
        };
        auto redactedPassEmptyUri = factory.makeRedactedUri();
        REQUIRE(redactedPassEmptyUri == expectedRedactedPassEmptyUri());

        auto passEmptyPool = factory.makePool();
        REQUIRE(passEmptyPool);
    }

    SECTION("Make a pool with client-side encryption enabled") {
        constexpr auto kSourceUri = "mongodb://127.0.0.1:27017";
        constexpr auto kEncryptedColls = R"({
          Encryption: {
            EncryptedCollections: [
                { Database: 'accounts',
                  Collection: 'balances',
                  EncryptionType: 'fle',
                  FLEEncryptedFields: {
                    name: {type: "string", algorithm: "random", keyId: "7aa359e0-1cdd-11ed-a2cd-bf985b6c5087"},
                    amount: {type: "int", algorithm: "deterministic", keyId: "8936e9ea-1cdd-11ed-be0d-b3f21cd2701f"}
                  }
                },
                { Database: 'accounts',
                  Collection: 'ratings',
                  EncryptionType: 'fle',
                  FLEEncryptedFields: {
                    ssn: {type: "string", algorithm: "random", keyId: "8936e9ea-1cdd-11ed-be0d-b3f21cd2701f"},
                    score: {type: "int", algorithm: "random", keyId: "7aa359e0-1cdd-11ed-a2cd-bf985b6c5087"}
                  }
                }
            ]
          }
        })";
        constexpr auto kEncryptionOpts = R"({
            KeyVaultDatabase: 'keyvault_db',
            KeyVaultCollection: 'datakeys',
            EncryptedCollections: [ 'accounts.balances', 'accounts.ratings' ]
        })";

        genny::NodeSource collsNs{kEncryptedColls, ""};
        genny::NodeSource optsNs{kEncryptionOpts, ""};

        auto factory = genny::v1::PoolFactory(kSourceUri);
        auto manager = genny::v1::EncryptionManager(collsNs.root(), true);

        auto encryption = manager.createEncryptionContext(kSourceUri, optsNs.root());

        factory.setEncryptionContext(encryption);

        auto factoryOpts = factory.makeOptions();
        REQUIRE(factoryOpts.client_opts().auto_encryption_opts().has_value());

        auto autoEncOpts = *factoryOpts.client_opts().auto_encryption_opts();
        REQUIRE(autoEncOpts.key_vault_namespace().has_value());
        REQUIRE(autoEncOpts.key_vault_namespace().value() == encryption.getKeyVaultNamespace());
        REQUIRE(autoEncOpts.kms_providers().has_value());
        REQUIRE(autoEncOpts.kms_providers().value() == encryption.generateKMSProvidersDoc());
        REQUIRE(autoEncOpts.schema_map().has_value());
        REQUIRE(autoEncOpts.schema_map().value() == encryption.generateSchemaMapDoc());
        REQUIRE(autoEncOpts.extra_options().has_value());
        REQUIRE(autoEncOpts.extra_options().value() == encryption.generateExtraOptionsDoc());

        auto pool = factory.makePool();
        REQUIRE(pool);
    }

    SECTION("PoolManager can construct multiple pools") {
        genny::v1::PoolManager manager{{}, true};
        genny::NodeSource ns{"Clients: {Default: {URI: 'mongodb:://localhost:27017', NoPreWarm: false}, Foo: {URI: 'mongodb:://localhost:27017', NoPreWarm: false}, Bar: {URI: 'mongodb:://localhost:27018', NoPreWarm: false}}", ""};
        auto& config = ns.root();

        auto foo0 = manager.createClient("Foo", 0, config);
        auto foo0again = manager.createClient("Foo", 0, config);
        auto foo10 = manager.createClient("Foo", 10, config);
        auto bar0 = manager.createClient("Bar", 0, config);

        // Note to future maintainers:
        //
        // This assertion doesn't actually verify that we aren't calling
        // `createPool()` again when running `manager.createClient("Foo", 0, config)` a
        // second time.
        //
        // A different style of trying to write this test is to register a
        // callback which gets called by `createPool()` and use that to "spy on"
        // the `name` and `instance` for which `createPool()` gets called.
        // Something like TIG-1191 would probably be helpful.
        REQUIRE((manager.instanceCount() ==
                 std::unordered_map<std::string, size_t>({{"Foo", 2}, {"Bar", 1}})));
    }

    SECTION("Make DNS seed list connection uri pools") {
        constexpr auto kSourceUri = "mongodb+srv://test.mongodb.net";

        auto factory = genny::v1::PoolFactory(kSourceUri);

        auto factoryUri = factory.makeUri();
        auto expectedUri = [&]() { return kSourceUri + std::string{"/?appName=Genny"}; };
        REQUIRE(factoryUri == expectedUri());
        auto redactedUri = factory.makeRedactedUri();
        REQUIRE(redactedUri == expectedUri());
    }
}
