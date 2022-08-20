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

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types/bson_value/view_or_value.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
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

    virtual void appendEncryptInfo(bsoncxx::builder::basic::sub_document subdoc) const = 0;

protected:
    std::string _path;
    std::string _type;
    std::optional<bsoncxx::types::bson_value::view_or_value> _keyId;
};

class FLEEncryptedField : public EncryptedField {
public:
    inline static const std::string kParentNodeName = "FLEEncryptedFields";

    FLEEncryptedField(const Node& yaml);

    FLEEncryptedField& algorithm(std::string algorithm);
    const std::string& algorithm() const;

    void appendEncryptInfo(bsoncxx::builder::basic::sub_document subdoc) const override;

private:
    std::string _algorithm;
};

template <EncryptionType enctype, class FieldType>
class EncryptedCollection {
public:
    using EncryptedFieldMap = std::unordered_map<std::string, FieldType>;

    EncryptedCollection(const Node& yaml) : _type(enctype) {
        _database = yaml["Database"].to<std::string>();
        _collection = yaml["Collection"].to<std::string>();
        if (_database.empty()) {
            throw InvalidConfigurationException(
                "'EncryptedCollection' requires a non-empty 'Database' name.");
        }
        if (_collection.empty()) {
            throw InvalidConfigurationException(
                "'EncryptedCollection' requires a non-empty 'Collection' name.");
        }

        const auto& fieldsNodeName = FieldType::kParentNodeName;

        const auto& fieldsNode = yaml[fieldsNodeName];
        if (fieldsNode) {
            if (!fieldsNode.isMap()) {
                throw InvalidConfigurationException("'" + fieldsNodeName +
                                                    "' node must be of map type");
            }
            for (const auto& [_, v] : fieldsNode) {
                addField(v.template to<FieldType>());
            }
        }
    }

    EncryptedCollection& database(std::string dbName) {
        _database = std::move(dbName);
        return *this;
    }
    const std::string& database() const {
        return _database;
    }

    EncryptedCollection& collection(std::string collName) {
        _collection = std::move(collName);
        return *this;
    }
    const std::string& collection() const {
        return _collection;
    }

    EncryptionType encryptionType() const {
        return _type;
    }

    EncryptedCollection& fields(EncryptedFieldMap fields) {
        _fields = std::move(fields);
        return *this;
    }
    const EncryptedFieldMap& fields() const {
        return _fields;
    }

    EncryptedCollection& addField(FieldType field) {
        auto key = field.path();
        _fields.emplace(std::move(key), std::move(field));
        return *this;
    }

    virtual void createCollection(const mongocxx::client& client) const = 0;
    virtual void dropCollection(const mongocxx::client& client) const = 0;

protected:
    std::string _database;
    std::string _collection;
    EncryptionType _type;
    EncryptedFieldMap _fields;
};

class FLEEncryptedCollection : public EncryptedCollection<EncryptionType::FLE, FLEEncryptedField> {
public:
    FLEEncryptedCollection(const Node& yaml)
        : EncryptedCollection<EncryptionType::FLE, FLEEncryptedField>(yaml) {}
    void appendSchema(bsoncxx::builder::basic::sub_document builder) const;
    void createCollection(const mongocxx::client& client) const override;
    void dropCollection(const mongocxx::client& client) const override;
};

class EncryptionContext {
public:
    // maps a namespace string to an EncryptedCollectionT type
    template <class EncryptedCollectionT>
    using EncryptedCollectionMap =
        std::unordered_map<std::string, std::unique_ptr<EncryptedCollectionT>>;

    EncryptionContext(){};
    EncryptionContext(const Node& encryptionOptsNode, std::string uri);

    std::pair<std::string, std::string> getKeyVaultNamespace() const;

    void setupKeyVault();

    bsoncxx::document::value generateKMSProvidersDoc() const;
    bsoncxx::document::value generateSchemaMapDoc() const;
    bsoncxx::document::value generateExtraOptionsDoc() const;
    bool encryptionEnabled() const;

private:
    EncryptedCollectionMap<FLEEncryptedCollection> _fleCollections;
    std::string _uri;
    std::string _keyVaultDb;
    std::string _keyVaultColl;
};

}  // namespace genny::v1

#endif  // HEADER_57E34EAA_1A86_11ED_B130_FF2EEC94BC5D_INCLUDED
