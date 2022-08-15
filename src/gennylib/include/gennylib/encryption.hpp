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

#ifndef HEADER_57E34EAA_1A86_11ED_B130_FF2EEC94BC5D_INCLUDED
#define HEADER_57E34EAA_1A86_11ED_B130_FF2EEC94BC5D_INCLUDED

#include <optional>
#include <string>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types/bson_value/view_or_value.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/Node.hpp>

namespace genny::v1 {

enum class EncryptionType {
    UNENCRYPTED = 0,
    FLE,
    QUERYABLE,
};

class EncryptedField {
public:
    EncryptedField(const Node& yaml);

    EncryptedField& path(std::string name);
    const std::string& path() const;

    EncryptedField& type(std::string type);
    const std::string& type() const;

    EncryptedField& keyId(bsoncxx::types::bson_value::view_or_value keyId);
    std::optional<bsoncxx::types::bson_value::view_or_value> keyId() const;

    EncryptedField& algorithm(std::string algorithm);
    const std::string& algorithm() const;

    void appendFLEEncryptInfo(bsoncxx::builder::basic::sub_document subdoc) const;

private:
    std::string _path;
    std::string _type;
    std::string _algorithm;
    std::optional<bsoncxx::types::bson_value::view_or_value> _keyId;
};

class EncryptedCollection {
public:
    // maps a field path to an EncryptedField
    using EncryptedFieldMap = std::unordered_map<std::string, EncryptedField>;

    EncryptedCollection(const Node& yaml);

    EncryptedCollection& database(std::string dbName);
    const std::string& database() const;

    EncryptedCollection& collection(std::string collName);
    const std::string& collection() const;

    EncryptedCollection& encryptionType(EncryptionType type);
    EncryptionType encryptionType() const;

    EncryptedCollection& fields(EncryptedFieldMap fields);
    const EncryptedFieldMap& fields() const;

    EncryptedCollection& addField(EncryptedField field);

    void appendFLESchema(bsoncxx::builder::basic::sub_document builder) const;

private:
    std::string _database;
    std::string _collection;
    EncryptionType _type;
    EncryptedFieldMap _fields;
};

class EncryptionContext {
public:
    // maps a namespace string to an EncryptedCollection
    using EncryptedCollectionMap = std::unordered_map<std::string, EncryptedCollection>;

    EncryptionContext(){};
    EncryptionContext(const Node& encryptionOptsNode, std::string keyVaultUri);

    std::pair<std::string, std::string> getKeyVaultNamespace() const;
    void setupKeyVault();
    bsoncxx::document::value generateKMSProvidersDoc() const;
    bsoncxx::document::value generateSchemaMapDoc() const;
    bsoncxx::document::value generateExtraOptionsDoc() const;
    bool encryptionEnabled() const;

private:
    EncryptedCollectionMap _collections;
    std::string _keyVaultUri;
    std::string _keyVaultDb;
    std::string _keyVaultColl;
};

}  // namespace genny::v1

#endif  // HEADER_57E34EAA_1A86_11ED_B130_FF2EEC94BC5D_INCLUDED
