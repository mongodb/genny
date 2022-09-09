// Copyright 2022-present MongoDB Inc.
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

#include <catch2/catch.hpp>

#include <gennylib/v1/PoolManager.hpp>

#include <testlib/helpers.hpp>

using namespace genny::v1;
using Catch::Matchers::Contains;

namespace {
constexpr auto kSourceUri = "mongodb://127.0.0.1:27017";
}  // namespace

TEST_CASE("EncryptionOptions with invalid fields") {

    SECTION("EncryptionOptions without KeyVaultCollection") {
        std::string encryptionOpts = R"({
            KeyVaultCollection: 'datakeys',
            EncryptedCollections: []
        })";
        genny::NodeSource ns{encryptionOpts, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionOptions(ns.root()); }(),
            Catch::Matches("Invalid key 'KeyVaultDatabase': Tried to access node that doesn't "
                           "exist. On node with path '/KeyVaultDatabase': "));
    }
    SECTION("EncryptionOptions without KeyVaultDatabase") {
        std::string encryptionOpts = R"({
            KeyVaultDatabase: 'testdb',
            EncryptedCollections: []
        })";
        genny::NodeSource ns{encryptionOpts, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionOptions(ns.root()); }(),
            Catch::Matches("Invalid key 'KeyVaultCollection': Tried to access node that doesn't "
                           "exist. On node with path '/KeyVaultCollection': "));
    }
    SECTION("EncryptionOptions with empty KeyVaultCollection") {
        std::string encryptionOpts = R"({
            KeyVaultDatabase: 'test',
            KeyVaultCollection: '',
            EncryptedCollections: []
        })";
        genny::NodeSource ns{encryptionOpts, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionOptions(ns.root()); }(),
            Catch::Matches("'EncryptionOptions' requires a non-empty 'KeyVaultCollection' name"));
    }
    SECTION("EncryptionOptions with empty KeyVaultDatabase") {
        std::string encryptionOpts = R"({
            KeyVaultDatabase: '',
            KeyVaultCollection: 'datakeys',
            EncryptedCollections: []
        })";
        genny::NodeSource ns{encryptionOpts, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionOptions(ns.root()); }(),
            Catch::Matches("'EncryptionOptions' requires a non-empty 'KeyVaultDatabase' name"));
    }
    SECTION("EncryptionOptions with non-sequence EncryptedCollections") {
        std::string encryptionOpts = R"({
            KeyVaultDatabase: 'testdb',
            KeyVaultCollection: 'datakeys',
            EncryptedCollections: 'foo'
        })";
        genny::NodeSource ns{encryptionOpts, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionOptions(ns.root()); }(),
            Catch::Matches(
                "'EncryptionOptions' requires an 'EncryptedCollections' node of sequence type"));
    }
}
TEST_CASE("EncryptedCollections with invalid fields") {
    SECTION("Non-sequence EncryptedCollections") {
        std::string encryptedColls = R"({
            Encryption: { EncryptedCollections: 'foo' }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches("'Encryption.EncryptedCollections' node must be of sequence type"));
    }
    SECTION("EncryptedCollections with duplicate namespaces") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle' },
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle' }
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches(
                "Collection with namespace 'foo.bar' already exists in 'EncryptedCollections'"));
    }
    SECTION("EncryptedCollections with invalid EncryptionType") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'unencrypted' },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches("'EncryptedCollections.0' has an invalid 'EncryptionType' value of "
                           "'unencrypted'. Valid values are 'fle' and 'queryable'."));
    }
    SECTION("EncryptedCollections entry with missing Database key") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Collection: "bar",
                  EncryptionType: 'fle' },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches(
                "Invalid key 'Database': Tried to access node that doesn't "
                "exist. On node with path '/Encryption/EncryptedCollections/0/Database': "));
    }
    SECTION("EncryptedCollections entry with missing Collection key") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  EncryptionType: 'fle' },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches(
                "Invalid key 'Collection': Tried to access node that doesn't "
                "exist. On node with path '/Encryption/EncryptedCollections/0/Collection': "));
    }
    SECTION("EncryptedCollections entry with empty Database key") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "",
                  Collection: "bar",
                  EncryptionType: 'fle' },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches("'EncryptedCollection' requires a non-empty 'Database' name."));
    }
    SECTION("EncryptedCollections entry with empty Collection key") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "",
                  EncryptionType: 'fle' },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches("'EncryptedCollection' requires a non-empty 'Collection' name."));
    }
    SECTION("EncryptedCollections entry with non-map type FLEEncryptedFields") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: [] },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH([&]() { EncryptionManager(ns.root(), true); }(),
                            Catch::Matches("'FLEEncryptedFields' node must be of map type"));
    }
    SECTION("FLEEncryptedFields entry with invalid path as key") {
        std::vector<std::string> badPaths = {
            "middle..empty",
            "ends.with.dot.",
            ".starts.with.dot",
            ".foo.",
            "..",
            ".",
        };
        const std::string encryptedCollsPrefix = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: {)";
        const std::string encryptedCollsSuffix =
            R"(: { type: "string", algorithm: "random" }} }]}})";

        for (auto& path : badPaths) {
            auto encryptedColls = encryptedCollsPrefix + path + encryptedCollsSuffix;
            genny::NodeSource ns{encryptedColls, ""};
            REQUIRE_THROWS_WITH([&]() { EncryptionManager(ns.root(), true); }(),
                                Catch::Matches("Field path \"" + path + "\" is not a valid path"));
        }
    }
    SECTION("FLEEncryptedFields entry with missing type") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: { field1 : { algorithm: "random" }} },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches(
                "Invalid key 'type': Tried to access node that doesn't "
                "exist. On node with path "
                "'/Encryption/EncryptedCollections/0/FLEEncryptedFields/field1/type': "));
    }
    SECTION("FLEEncryptedFields entry with empty keyId") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: { field1 : { type: "string", algorithm: "random", keyId: "" }} },
            ]
            }
        })";

        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH([&]() { EncryptionManager(ns.root(), true); }(),
                            Catch::Matches("'EncryptedField' has an invalid 'keyId' value of ''. "
                                           "Value must be a UUID string."));
    }
    SECTION("FLEEncryptedFields entry with missing algorithm") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: { field1 : { type: "string" }} },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches(
                "Invalid key 'algorithm': Tried to access node that doesn't "
                "exist. On node with path "
                "'/Encryption/EncryptedCollections/0/FLEEncryptedFields/field1/algorithm': "));
    }
    SECTION("FLEEncryptedFields entry with invalid algorithm") {
        std::string encryptedColls = R"({
            Encryption: {
            EncryptedCollections: [
                { Database: "foo",
                  Collection: "bar",
                  EncryptionType: 'fle',
                  FLEEncryptedFields: { field1 : { type: "string", algorithm: "equality" }} },
            ]
            }
        })";
        genny::NodeSource ns{encryptedColls, ""};
        REQUIRE_THROWS_WITH(
            [&]() { EncryptionManager(ns.root(), true); }(),
            Catch::Matches("'field1' has an invalid 'algorithm' value of 'equality'. "
                           "Valid values are 'random' and 'deterministic'."));
    }
}
TEST_CASE("No CryptSharedLibPath when UseCryptSharedLib is true") {
    const std::string errmsg =
        "A non-empty Encryption.CryptSharedLibPath is required if "
        "Encryption.UseCryptSharedLib is true";

    SECTION("CryptSharedLibPath is empty") {
        std::string encryption =
            R"({Encryption: { UseCryptSharedLib: true, CryptSharedLibPath: "" }})";
        genny::NodeSource ns{encryption, ""};
        REQUIRE_THROWS_WITH([&]() { EncryptionManager(ns.root(), true); }(),
                            Catch::Matches(errmsg));
    }
    SECTION("CryptSharedLibPath is absent") {
        std::string encryption = R"({Encryption: { UseCryptSharedLib: true }})";
        genny::NodeSource ns{encryption, ""};
        REQUIRE_THROWS_WITH([&]() { EncryptionManager(ns.root(), true); }(),
                            Catch::Matches(errmsg));
    }
}
TEST_CASE("EncryptionOptions with non-existing namespace") {
    std::string encryptedColls = R"({ Encryption: { EncryptedCollections: [] }})";
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'testdb',
        KeyVaultCollection: 'testcoll',
        EncryptedCollections: [ 'foo.collection' ]
    })";
    genny::NodeSource optsNs{encryptionOpts, ""};
    genny::NodeSource collsNs{encryptedColls, ""};

    EncryptionManager mgr(collsNs.root(), true);

    REQUIRE_THROWS_WITH(
        [&]() { mgr.createEncryptionContext(kSourceUri, EncryptionOptions(optsNs.root())); }(),
        Catch::Matches("No encrypted collection schema found with namespace 'foo.collection'"));
}
TEST_CASE("createEncryptionContext with empty EncryptionOptions creates empty EncryptionContext") {
    std::string encryptedColls = R"({ Encryption: { EncryptedCollections: [] }})";
    genny::NodeSource collsNs{encryptedColls, ""};
    EncryptionManager mgr(collsNs.root(), true);
    auto encContext = mgr.createEncryptionContext(kSourceUri, EncryptionOptions{});
    REQUIRE(!encContext.hasEncryptedCollections());
    REQUIRE(encContext.getKeyVaultNamespace().first.empty());
    REQUIRE(encContext.getKeyVaultNamespace().second.empty());

    auto schemaDoc = encContext.generateSchemaMapDoc();
    REQUIRE(schemaDoc == bsoncxx::from_json("{}"));
}
TEST_CASE("EncryptionContext outputs correct key vault namespace") {
    std::string encryptedColls = R"({ Encryption: { EncryptedCollections: [] }})";
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'testdb',
        KeyVaultCollection: 'datakeys',
        EncryptedCollections: []
    })";
    genny::NodeSource collsNs{encryptedColls, ""};
    genny::NodeSource optsNs{encryptionOpts, ""};
    EncryptionManager mgr(collsNs.root(), true);

    auto encryption = mgr.createEncryptionContext(kSourceUri, optsNs.root());
    auto nspair = encryption.getKeyVaultNamespace();
    REQUIRE(nspair.first == "testdb");
    REQUIRE(nspair.second == "datakeys");
}
TEST_CASE("EncryptionContext outputs correct local KMS providers document") {
    std::string encryptedColls = R"({ Encryption: { EncryptedCollections: [] }})";
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'testdb',
        KeyVaultCollection: 'datakeys',
        EncryptedCollections: []
    })";
    genny::NodeSource collsNs{encryptedColls, ""};
    genny::NodeSource optsNs{encryptionOpts, ""};
    EncryptionManager mgr(collsNs.root(), true);

    auto encryption = mgr.createEncryptionContext(kSourceUri, optsNs.root());
    auto doc = encryption.generateKMSProvidersDoc();
    REQUIRE(doc.view()["local"]);
    REQUIRE(doc.view()["local"]["key"]);
    REQUIRE_NOTHROW(doc.view()["local"]["key"].get_binary());
}
TEST_CASE("EncryptionContext outputs correct extra options document") {
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'testdb',
        KeyVaultCollection: 'datakeys',
        EncryptedCollections: []
    })";
    genny::NodeSource optsNs{encryptionOpts, ""};

    SECTION("UseCryptSharedLib is false") {
        std::string encryptedColls = R"({ Encryption: { EncryptedCollections: [] }})";
        genny::NodeSource collsNs{encryptedColls, ""};
        EncryptionManager mgr(collsNs.root(), true);

        auto encryption = mgr.createEncryptionContext(kSourceUri, optsNs.root());
        auto doc = encryption.generateExtraOptionsDoc();
        REQUIRE(doc.view()["mongocryptdBypassSpawn"]);
        REQUIRE_NOTHROW(doc.view()["mongocryptdBypassSpawn"].get_bool());
        REQUIRE(doc.view()["mongocryptdBypassSpawn"].get_bool());
        REQUIRE(doc.view()["cryptSharedLibRequired"]);
        REQUIRE_NOTHROW(doc.view()["cryptSharedLibRequired"].get_bool());
        REQUIRE(doc.view()["cryptSharedLibRequired"].get_bool() == false);
    }
    SECTION("UseCryptSharedLib is true") {
        std::string encryptedColls = R"({
            Encryption: { UseCryptSharedLib: true, CryptSharedLibPath: "/usr/lib/mongo_crypt_v1.so"}
        })";
        genny::NodeSource collsNs{encryptedColls, ""};
        EncryptionManager mgr(collsNs.root(), false);

        // Since dryRun is false, create an EncryptionContext directly instead of calling
        // createEncryptionContext().
        EncryptionContext encryption(EncryptionOptions(optsNs.root()), kSourceUri, mgr);

        auto doc = encryption.generateExtraOptionsDoc();
        REQUIRE(doc.view()["mongocryptdBypassSpawn"]);
        REQUIRE_NOTHROW(doc.view()["mongocryptdBypassSpawn"].get_bool());
        REQUIRE(doc.view()["mongocryptdBypassSpawn"].get_bool());
        REQUIRE(doc.view()["cryptSharedLibRequired"]);
        REQUIRE_NOTHROW(doc.view()["cryptSharedLibRequired"].get_bool());
        REQUIRE(doc.view()["cryptSharedLibRequired"].get_bool());
        REQUIRE(doc.view()["cryptSharedLibPath"]);
        REQUIRE_NOTHROW(doc.view()["cryptSharedLibPath"].get_utf8());
        REQUIRE(doc.view()["cryptSharedLibPath"].get_utf8().value == "/usr/lib/mongo_crypt_v1.so");
    }
}
TEST_CASE("EncryptionContext outputs correct schema map document") {
    std::string encryptedColls = R"({
        Encryption: {
        EncryptedCollections: [
            { Database: 'accounts',
                Collection: 'balances',
                EncryptionType: 'fle',
                FLEEncryptedFields: {
                name: {type: "string", algorithm: "random", keyId: "7aa359e0-1cdd-11ed-a2cd-bf985b6c5087"},
                "pii.ssn": {type: "string", algorithm: "deterministic", keyId: "8936e9ea-1cdd-11ed-be0d-b3f21cd2701f"},
                "pii.dob": {type: "int", algorithm: "deterministic", keyId: "ffeeddba-1cdd-11ed-be0d-b3f21cd2701f"}
                }
            }
        ]
        }
    })";
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'keyvault_db',
        KeyVaultCollection: 'datakeys',
        EncryptedCollections: [ 'accounts.balances' ]
    })";
    std::string expectedJson = R"({
        "accounts.balances" : {
            "properties" : {
                "pii" : {
                    "properties" : {
                        "dob" : {
                            "encrypt" : {
                                "bsonType" : "int",
                                "algorithm" : "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic",
                                "keyId" : [ { "$binary" : "/+7duhzdEe2+DbPyHNJwHw==", "$type" : "04" } ]
                            }
                        },
                        "ssn" : {
                            "encrypt" : {
                                "bsonType" : "string",
                                "algorithm" : "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic",
                                "keyId" : [ { "$binary" : "iTbp6hzdEe2+DbPyHNJwHw==", "$type" : "04" } ]
                            }
                        }
                    },
                    "bsonType" : "object"
                },
                "name" : {
                    "encrypt" : {
                        "bsonType" : "string",
                        "algorithm" : "AEAD_AES_256_CBC_HMAC_SHA_512-Random",
                        "keyId" : [ { "$binary" : "eqNZ4BzdEe2izb+YW2xQhw==", "$type" : "04" } ]
                    }
                }
            },
            "bsonType" : "object"
        }
    })";
    genny::NodeSource collsNs{encryptedColls, ""};
    genny::NodeSource optsNs{encryptionOpts, ""};
    EncryptionManager mgr(collsNs.root(), true);

    auto encryption = mgr.createEncryptionContext(kSourceUri, optsNs.root());

    auto doc = encryption.generateSchemaMapDoc();
    auto expectedDoc = bsoncxx::from_json(expectedJson);
    REQUIRE(doc == expectedDoc);
}
TEST_CASE("EncryptionContext outputs correct auto_encryption options") {
    std::string encryptedColls = R"({
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
    std::string encryptionOpts = R"({
        KeyVaultDatabase: 'keyvault_db',
        KeyVaultCollection: 'datakeys',
        EncryptedCollections: [ 'accounts.balances', 'accounts.ratings' ]
    })";
    genny::NodeSource collsNs{encryptedColls, ""};
    genny::NodeSource optsNs{encryptionOpts, ""};
    EncryptionManager mgr(collsNs.root(), true);

    auto encryption = mgr.createEncryptionContext(kSourceUri, optsNs.root());

    auto opts = encryption.getAutoEncryptionOptions();

    REQUIRE(opts.key_vault_namespace().has_value());
    REQUIRE(opts.key_vault_namespace().value().first == "keyvault_db");
    REQUIRE(opts.key_vault_namespace().value().second == "datakeys");

    REQUIRE(opts.kms_providers().has_value());
    const auto& kmsdoc = opts.kms_providers().value();
    REQUIRE(kmsdoc.view()["local"]);
    REQUIRE(kmsdoc.view()["local"]["key"]);
    REQUIRE_NOTHROW(kmsdoc.view()["local"]["key"].get_binary());

    REQUIRE(opts.schema_map().has_value());
    auto& schemaDoc = opts.schema_map().value();
    REQUIRE(schemaDoc.view()["accounts.balances"]);
    REQUIRE(schemaDoc.view()["accounts.ratings"]);

    REQUIRE(opts.extra_options().has_value());
    const auto& extradoc = opts.extra_options().value();
    REQUIRE(extradoc.view()["mongocryptdBypassSpawn"]);
    REQUIRE_NOTHROW(extradoc.view()["mongocryptdBypassSpawn"].get_bool());
    REQUIRE(extradoc.view()["mongocryptdBypassSpawn"].get_bool());
    REQUIRE(extradoc.view()["cryptSharedLibRequired"]);
    REQUIRE_NOTHROW(extradoc.view()["cryptSharedLibRequired"].get_bool());
    REQUIRE(extradoc.view()["cryptSharedLibRequired"].get_bool() == false);
}
