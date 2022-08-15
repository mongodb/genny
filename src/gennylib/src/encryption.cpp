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

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/client_encryption.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/encryption.hpp>

namespace {

/**
 * This struct represents a node in the tree-like structure
 * of an FLE schema.
 */
struct FLEFieldPathNode {
    std::string name;
    std::vector<FLEFieldPathNode> subnodes;
    const genny::v1::EncryptedField* field;

    /**
     * Helper function for building the FLE schema document, which has a
     * hierarchical structure.
     *
     * @param subdoc the document builder to append fields onto
     * @param root whether this node is the root, which does not have a name
     */
    void append(bsoncxx::builder::basic::sub_document subdoc, bool root = false) const {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::sub_document;

        auto appendProperties = [](sub_document fieldDoc,
                                   const std::vector<FLEFieldPathNode>& childNodes) {
            fieldDoc.append(kvp("properties", [&](sub_document propDoc) {
                for (auto& n : childNodes) {
                    n.append(propDoc);
                }
            }));
            fieldDoc.append(kvp("bsonType", "object"));
        };

        if (subnodes.empty()) {
            subdoc.append(kvp(name, [&](sub_document fieldDoc) {
                fieldDoc.append(kvp("encrypt", [&](sub_document encryptDoc) {
                    field->appendFLEEncryptInfo(encryptDoc);
                }));
            }));
        } else if (!root) {
            subdoc.append(
                kvp(name, [&](sub_document fieldDoc) { appendProperties(fieldDoc, subnodes); }));
        } else {
            appendProperties(subdoc, subnodes);
        }
    }
};

/**
 * Splits a dot-separated path string into its constituent parts &
 * returns a list.
 *
 * @param path the dotted path string
 * @return a non-empty ordered list of parsed path components
 * @throws InvalidConfigurationException
 *   if path is empty, or has empty path segments
 */
std::vector<std::string> splitDottedPath(const std::string& path) {
    std::vector<std::string> parts;
    for (size_t start = 0; start != std::string::npos;) {
        auto dot = path.find('.', start);
        if (dot != std::string::npos) {
            parts.push_back(path.substr(start, dot - start));
            start = dot + 1;
        } else {
            parts.push_back(path.substr(start));
            start = dot;
        }
        if (parts.back().empty()) {
            std::ostringstream ss;
            ss << "Field path \"" << path << "\" is not a valid path";
            throw genny::InvalidConfigurationException(ss.str());
        }
    }
    return parts;
}

}  // namespace

namespace genny::v1 {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::sub_array;
using bsoncxx::builder::basic::sub_document;
using bsoncxx::types::bson_value::view_or_value;

EncryptedField::EncryptedField(const Node& yaml) {
    _path = yaml.key();
    _type = yaml["type"].to<std::string>();
    _algorithm = yaml["algorithm"].to<std::string>();

    if (_algorithm == "random") {
        _algorithm = "AEAD_AES_256_CBC_HMAC_SHA_512-Random";
    } else if (_algorithm == "deterministic") {
        _algorithm = "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic";
    } else {
        std::ostringstream ss;
        ss << "'EncryptedField' has an invalid 'algorithm' value of '" << _algorithm
           << "'. Valid values are 'random' and 'deterministic'.";
        throw InvalidConfigurationException(ss.str());
    }

    if (yaml["keyId"]) {
        auto keyId = yaml["keyId"].to<std::string>();
        try {
            auto jsonDoc = R"({"v": {"$uuid": ")" + keyId + R"("}})";
            auto uuidDoc = bsoncxx::from_json(jsonDoc);
            _keyId = uuidDoc.view()["v"].get_owning_value();
        } catch (const bsoncxx::exception& e) {
            std::ostringstream ss;
            ss << "'EncryptedField' has an invalid 'keyId' value of '" << keyId
               << "'. Value must a UUID string.";
            throw InvalidConfigurationException(ss.str());
        }
    }
}

EncryptedField& EncryptedField::path(std::string path) {
    _path = std::move(path);
    return *this;
}

const std::string& EncryptedField::path() const {
    return _path;
}

EncryptedField& EncryptedField::type(std::string type) {
    _type = std::move(type);
    return *this;
}

const std::string& EncryptedField::type() const {
    return _type;
}

EncryptedField& EncryptedField::keyId(view_or_value keyId) {
    _keyId = std::move(keyId);
    return *this;
}

std::optional<view_or_value> EncryptedField::keyId() const {
    return _keyId;
}

EncryptedField& EncryptedField::algorithm(std::string algorithm) {
    _algorithm = std::move(algorithm);
    return *this;
}

const std::string& EncryptedField::algorithm() const {
    return _algorithm;
}

void EncryptedField::appendFLEEncryptInfo(sub_document subdoc) const {
    subdoc.append(kvp("keyId", [&](sub_array keyIdArray) {
        if (!_keyId.has_value()) {
            std::ostringstream ss;
            ss << "EncryptedField '" << _path << "' has no key ID.";
            throw InvalidConfigurationException(ss.str());
        }
        keyIdArray.append(_keyId.value());
    }));
    subdoc.append(kvp("bsonType", _type));
    subdoc.append(kvp("algorithm", _algorithm));
}

EncryptedCollection::EncryptedCollection(const Node& yaml) {
    _database = yaml["Database"].to<std::string>();
    _collection = yaml["Collection"].to<std::string>();
    auto typeStr = yaml["EncryptionType"].to<std::string>();

    if (_database.empty()) {
        throw InvalidConfigurationException(
            "'EncryptedCollection' requires a non-empty 'Database' name.");
    }
    if (_collection.empty()) {
        throw InvalidConfigurationException(
            "'EncryptedCollection' requires a non-empty 'Collection' name.");
    }

    if (typeStr == "fle") {
        _type = EncryptionType::FLE;
    } else if (typeStr == "queryable") {
        _type = EncryptionType::QUERYABLE;
    } else {
        std::ostringstream ss;
        ss << "'EncryptedCollection' has an invalid 'EncryptionType' value of '" << typeStr
           << "'. Valid values are 'fle' and 'queryable'.";
        throw InvalidConfigurationException(ss.str());
    }

    const auto& fieldsNode = yaml["EncryptedFields"];
    if (fieldsNode) {
        if (!fieldsNode.isMap()) {
            throw InvalidConfigurationException(
                "'EncryptedCollection' requires an 'EncryptedFields' node of map type.");
        }
        for (const auto& [_, v] : fieldsNode) {
            addField(v.to<EncryptedField>());
        }
    }
}

EncryptedCollection& EncryptedCollection::database(std::string dbName) {
    _database = std::move(dbName);
    return *this;
}

const std::string& EncryptedCollection::database() const {
    return _database;
}

EncryptedCollection& EncryptedCollection::collection(std::string collName) {
    _collection = std::move(collName);
    return *this;
}

const std::string& EncryptedCollection::collection() const {
    return _collection;
}

EncryptedCollection& EncryptedCollection::encryptionType(EncryptionType type) {
    _type = type;
    return *this;
}

EncryptionType EncryptedCollection::encryptionType() const {
    return _type;
}

EncryptedCollection& EncryptedCollection::fields(EncryptedCollection::EncryptedFieldMap fields) {
    _fields = std::move(fields);
    return *this;
}

const EncryptedCollection::EncryptedFieldMap& EncryptedCollection::fields() const {
    return _fields;
}

EncryptedCollection& EncryptedCollection::addField(EncryptedField field) {
    auto key = field.path();
    _fields.emplace(std::move(key), std::move(field));
    return *this;
}

void EncryptedCollection::appendFLESchema(sub_document subdoc) const {
    if (_fields.empty()) {
        return;
    }

    // Since the FLE schema has a nested structure, we first build a prefix tree
    // from the list of dotted field paths obtained from _fields.
    // Fail if two field paths are in conflict, meaning that one is a prefix of another
    // vice-versa.
    FLEFieldPathNode root{{}, {}, nullptr};

    for (auto& [_, field] : _fields) {
        FLEFieldPathNode* level = &root;
        auto parts = splitDottedPath(field.path());

        // For each part of the path, follow the branch of the tree that matches.
        // Add new branches if there are none that match the path.
        for (auto& part : parts) {
            auto nitr = std::find_if(level->subnodes.begin(),
                                     level->subnodes.end(),
                                     [&](auto& node) { return node.name == part; });
            if (nitr != level->subnodes.end()) {
                // Found a branch that matches the current part
                // Check for a conflict: fail if the matching node is a leaf
                // or the current part is the last one.
                if (nitr->subnodes.empty() || &part == &parts.back()) {
                    std::ostringstream ss;
                    ss << "'EncryptedField' with path '" << field.path()
                       << "' conflicts with another 'EncryptedField' with path '"
                       << nitr->field->path() << "'";
                    throw InvalidConfigurationException(ss.str());
                }
                level = &(*nitr);
            } else {
                // No matching branch; add new branch
                level->subnodes.push_back(FLEFieldPathNode{part, {}, &field});
                level = &(level->subnodes.back());
            }
        }
    }

    root.append(subdoc, true);
}

EncryptionContext::EncryptionContext(const Node& encryptionOptsNode, std::string keyVaultUri)
    : _keyVaultUri(std::move(keyVaultUri)) {

    if (!encryptionOptsNode) {
        return;
    }

    _keyVaultDb = encryptionOptsNode["KeyVaultDatabase"].to<std::string>();
    _keyVaultColl = encryptionOptsNode["KeyVaultCollection"].to<std::string>();

    if (_keyVaultDb.empty()) {
        throw InvalidConfigurationException(
            "'EncryptionOptions' requires a non-empty 'KeyVaultDatabase' name");
    }
    if (_keyVaultColl.empty()) {
        throw InvalidConfigurationException(
            "'EncryptionOptions' requires a non-empty 'KeyVaultCollection' name");
    }

    const auto& collsSequence = encryptionOptsNode["EncryptedCollections"];
    if (!collsSequence) {
        return;
    }
    if (!collsSequence.isSequence()) {
        throw InvalidConfigurationException("'EncryptedCollections' node must be a sequence type.");
    }
    for (const auto& [_, v] : collsSequence) {
        auto coll = v.to<EncryptedCollection>();
        auto key = coll.database() + "." + coll.collection();
        if (!_collections.emplace(std::move(key), std::move(coll)).second) {
            std::ostringstream ss;
            ss << "Collection with namespace '" << key
               << "' already exists in 'EncryptionCollections'";
            throw InvalidConfigurationException(ss.str());
        }
    }
}

void EncryptionContext::setupKeyVault() {
    if (!encryptionEnabled()) {
        return;
    }

    mongocxx::uri uri{_keyVaultUri};
    mongocxx::client client{uri};
    auto coll = client[_keyVaultDb][_keyVaultColl];
    auto kvNs = _keyVaultDb + "." + _keyVaultColl;

    BOOST_LOG_TRIVIAL(info) << "Setting up key vault at namespace '" << kvNs << "'";

    // Drop any existing key vaults
    coll.drop();

    // Create unique index to ensure that two data keys cannot share the same
    // keyAltName. This is recommended practice for the key vault.
    mongocxx::options::index indexOptions{};
    indexOptions.unique(true);
    auto expression = make_document(
        kvp("keyAltNames", [&](sub_document subdoc) { subdoc.append(kvp("$exists", true)); }));
    indexOptions.partial_filter_expression(expression.view());
    coll.create_index(make_document(kvp("keyAltNames", 1)), indexOptions);

    mongocxx::options::client_encryption clientEncryptionOpts;
    clientEncryptionOpts.key_vault_namespace(getKeyVaultNamespace());
    clientEncryptionOpts.kms_providers(generateKMSProvidersDoc());
    clientEncryptionOpts.key_vault_client(&client);
    mongocxx::client_encryption clientEncryption{std::move(clientEncryptionOpts)};

    // For each encrypted collection, create the data keys to be used for encrypting
    // the values for each encrypted field in that collection. Update the keyIds of
    // each encrypted field with the UUID of the newly-created data key.
    for (auto& [_, encryptedColl] : _collections) {
        auto fieldsCopy = encryptedColl.fields();
        for (auto& [_, field] : fieldsCopy) {
            try {
                field.keyId(clientEncryption.create_data_key("local"));
                BOOST_LOG_TRIVIAL(debug)
                    << "Data key created on key vault '" << kvNs << "': "
                    << bsoncxx::to_json(make_document(kvp("keyId", field.keyId().value())));
            } catch (const mongocxx::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "Failed to create data key on key vault '" << kvNs
                                         << "' at '" << _keyVaultUri << ": " << e.what();
                throw e;
            }
        }
        encryptedColl.fields(std::move(fieldsCopy));
    }
};

bsoncxx::document::value EncryptionContext::generateKMSProvidersDoc() const {
    return make_document(kvp("local", [&](sub_document localDoc) {
        constexpr uint8_t localMasterKey[] = {
            0x77, 0x1f, 0x2d, 0x7d, 0x76, 0x74, 0x39, 0x08, 0x50, 0x0b, 0x61, 0x14, 0x3a, 0x07,
            0x24, 0x7c, 0x37, 0x7b, 0x60, 0x0f, 0x09, 0x11, 0x23, 0x65, 0x35, 0x01, 0x3a, 0x76,
            0x5f, 0x3e, 0x4b, 0x6a, 0x65, 0x77, 0x21, 0x6d, 0x34, 0x13, 0x24, 0x1b, 0x47, 0x73,
            0x21, 0x5d, 0x56, 0x6a, 0x38, 0x30, 0x6d, 0x5e, 0x79, 0x1b, 0x25, 0x4d, 0x2a, 0x00,
            0x7c, 0x0b, 0x65, 0x1d, 0x70, 0x22, 0x22, 0x61, 0x2e, 0x6a, 0x52, 0x46, 0x6a, 0x43,
            0x43, 0x23, 0x58, 0x21, 0x78, 0x59, 0x64, 0x35, 0x5c, 0x23, 0x00, 0x27, 0x43, 0x7d,
            0x50, 0x13, 0x65, 0x3c, 0x54, 0x1e, 0x74, 0x3c, 0x3b, 0x57, 0x21, 0x1a};
        localDoc.append(kvp("key",
                            bsoncxx::types::b_binary{bsoncxx::binary_sub_type::k_binary,
                                                     sizeof(localMasterKey),
                                                     localMasterKey}));
    }));
}

std::pair<std::string, std::string> EncryptionContext::getKeyVaultNamespace() const {
    return {_keyVaultDb, _keyVaultColl};
}

bsoncxx::document::value EncryptionContext::generateSchemaMapDoc() const {
    bsoncxx::builder::basic::document mapDoc{};
    for (auto& [nss, coll] : _collections) {
        if (coll.encryptionType() != EncryptionType::FLE) {
            continue;
        }
        mapDoc.append(kvp(nss, [&](sub_document subdoc) { coll.appendFLESchema(subdoc); }));
    }
    auto v = mapDoc.extract();
    BOOST_LOG_TRIVIAL(debug) << "Generated schema map: " << bsoncxx::to_json(v);
    return std::move(v);
}

bsoncxx::document::value EncryptionContext::generateExtraOptionsDoc() const {
    return make_document(kvp("mongocryptdBypassSpawn", true));
}

bool EncryptionContext::encryptionEnabled() const {
    return !_collections.empty();
}

}  // namespace genny::v1
