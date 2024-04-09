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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>
#include <boost/functional/hash.hpp>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/decimal128.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/client_encryption.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/v1/PoolManager.hpp>

namespace genny::v1 {

enum class EncryptionType {
    UNENCRYPTED = 0,
    FLE,
    QUERYABLE,
};

class EncryptedField {
public:
    EncryptedField(const Node& yaml);

    EncryptedField& path(std::string path) {
        _path = std::move(path);
        return *this;
    }
    const std::string& path() const {
        return _path;
    }

    EncryptedField& type(std::string type) {
        _type = std::move(type);
        return *this;
    }
    const std::string& type() const {
        return _type;
    }

    EncryptedField& keyId(bsoncxx::types::bson_value::view_or_value keyId) {
        _keyId = std::move(keyId);
        return *this;
    }
    std::optional<bsoncxx::types::bson_value::view_or_value> keyId() const {
        return _keyId;
    }

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

    FLEEncryptedField& algorithm(std::string algorithm) {
        _algorithm = std::move(algorithm);
        return *this;
    }
    const std::string& algorithm() const {
        return _algorithm;
    }

    void appendEncryptInfo(bsoncxx::builder::basic::sub_document subdoc) const override;

private:
    std::string _algorithm;
};

class QueryableEncryptedField : public EncryptedField {
public:
    inline static const std::string kParentNodeName = "QueryableEncryptedFields";

    QueryableEncryptedField(const Node& yaml);

    void appendEncryptInfo(bsoncxx::builder::basic::sub_document subdoc) const override;

private:
    struct Query {
        Query(const Node& yaml);
        std::string queryType;
        std::optional<int> contention;
        std::optional<int> sparsity;
        std::optional<int> trimFactor;
        std::optional<int> precision;
        std::optional<std::string> min, max;
    };
    std::vector<Query> _queries;
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

    std::string namespaceString() const {
        return _database + "." + _collection;
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

class QueryableEncryptedCollection
    : public EncryptedCollection<EncryptionType::QUERYABLE, QueryableEncryptedField> {
public:
    QueryableEncryptedCollection(const Node& yaml)
        : EncryptedCollection<EncryptionType::QUERYABLE, QueryableEncryptedField>(yaml) {}
    void appendEncryptedFieldConfig(bsoncxx::builder::basic::sub_document builder) const;
    void createCollection(const mongocxx::client& client) const override;
    void dropCollection(const mongocxx::client& client) const override;
};

}  // namespace genny::v1

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
                    field->appendEncryptInfo(encryptDoc);
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
    boost::split(parts, path, boost::is_any_of("."));
    if (std::any_of(parts.begin(), parts.end(), [](auto& part) { return part.empty(); })) {
        std::ostringstream ss;
        ss << "Field path \"" << path << "\" is not a valid path";
        throw genny::InvalidConfigurationException(ss.str());
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

    // verify path is valid
    splitDottedPath(_path);

    if (yaml["keyId"]) {
        auto keyId = yaml["keyId"].to<std::string>();
        try {
            auto uuidDoc = bsoncxx::from_json(
                boost::str(boost::format(R"({"v": {"$uuid": "%1%" }})") % keyId));
            _keyId = uuidDoc.view()["v"].get_owning_value();
        } catch (const bsoncxx::exception& e) {
            std::ostringstream ss;
            ss << "'EncryptedField' has an invalid 'keyId' value of '" << keyId
               << "'. Value must be a UUID string.";
            throw InvalidConfigurationException(ss.str());
        }
    }
}

FLEEncryptedField::FLEEncryptedField(const Node& yaml) : EncryptedField(yaml) {
    _algorithm = yaml["algorithm"].to<std::string>();

    if (_algorithm == "random") {
        _algorithm = "AEAD_AES_256_CBC_HMAC_SHA_512-Random";
    } else if (_algorithm == "deterministic") {
        _algorithm = "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic";
    } else {
        std::ostringstream ss;
        ss << "'" << _path << "' has an invalid 'algorithm' value of '" << _algorithm
           << "'. Valid values are 'random' and 'deterministic'.";
        throw InvalidConfigurationException(ss.str());
    }
}

void FLEEncryptedField::appendEncryptInfo(sub_document subdoc) const {
    subdoc.append(kvp("bsonType", _type));
    subdoc.append(kvp("algorithm", _algorithm));
    if (_keyId.has_value()) {
        subdoc.append(
            kvp("keyId", [&](sub_array keyIdArray) { keyIdArray.append(_keyId.value()); }));
    }
}

QueryableEncryptedField::Query::Query(const Node& yaml) {
    queryType = yaml["queryType"].to<std::string>();
    contention = yaml["contention"].maybe<int>();
    sparsity = yaml["sparsity"].maybe<int>();
    trimFactor = yaml["trimFactor"].maybe<int>();
    precision = yaml["precision"].maybe<int>();
    min = yaml["min"].maybe<std::string>();
    max = yaml["max"].maybe<std::string>();
}

QueryableEncryptedField::QueryableEncryptedField(const Node& yaml) : EncryptedField(yaml) {
    const auto& queriesNode = yaml["queries"];
    if (!queriesNode) {
        return;
    }

    if (queriesNode.isSequence()) {
        for (auto& [_, queryNode] : queriesNode) {
            if (!queryNode.isMap()) {
                throw InvalidConfigurationException(
                    "Each value in the 'queries' array must be of map type");
            }
            _queries.emplace_back(queryNode);
        }
    } else if (queriesNode.isMap()) {
        _queries.emplace_back(queriesNode);
    } else {
        throw InvalidConfigurationException("'queries' node must be of sequence or map type");
    }
}

void QueryableEncryptedField::appendEncryptInfo(sub_document subdoc) const {
    subdoc.append(kvp("path", _path));
    if (_keyId.has_value()) {
        subdoc.append(kvp("keyId", _keyId.value()));
    }
    subdoc.append(kvp("bsonType", _type));

    if (_queries.empty()) {
        return;
    }

    subdoc.append(kvp("queries", [&](sub_array queriesArray) {
        for (auto& query : _queries) {
            queriesArray.append([&](sub_document queryDoc) {
                queryDoc.append(kvp("queryType", query.queryType));
                if (query.contention) {
                    queryDoc.append(kvp("contention", query.contention.value()));
                }

                auto append_string_as_number_with_correct_type =
                        [&](const std::string& s, const std::string& n) {
                    if(_type == "double") {
                        queryDoc.append(kvp(s, std::stod(n)));
                    } else if(_type == "decimal") {
                        queryDoc.append(kvp(s, bsoncxx::decimal128(n)));
                    } else if(_type == "long") {
                        queryDoc.append(kvp(s, int64_t(std::stol(n))));
                    } else if(_type == "int") {
                        queryDoc.append(kvp(s, std::stoi(n)));
                    } else {
                        // XXX Eventually might need to handle dates as well, depending on what
                        // tests we run.
                        std::ostringstream ss;
                        ss << "Expected number bsonType, but got: \"" << _type << "\"";
                        throw InvalidConfigurationException(ss.str());
                    }
                };
                if(query.queryType == "range") {
                    if(query.min) {
                        append_string_as_number_with_correct_type("min", query.min.value());
                    }
                    if(query.max) {
                        append_string_as_number_with_correct_type("max", query.max.value());
                    }
                    if(query.sparsity) {
                        queryDoc.append(kvp("sparsity", query.sparsity.value()));
                    }
                    if(query.trimFactor) {
                        queryDoc.append(kvp("trimFactor", query.trimFactor.value()));
                    }
                    if(query.precision) {
                        queryDoc.append(kvp("precision", query.precision.value()));
                    }
                }
            });
        }
    }));
}

void FLEEncryptedCollection::appendSchema(sub_document subdoc) const {
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
                    ss << "'FLEEncryptedField' with path '" << field.path()
                       << "' conflicts with another 'FLEEncryptedField' with path '"
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

void FLEEncryptedCollection::createCollection(const mongocxx::client& client) const {
    client[_database].create_collection(_collection);
}

void FLEEncryptedCollection::dropCollection(const mongocxx::client& client) const {
    client[_database][_collection].drop();
}

void QueryableEncryptedCollection::appendEncryptedFieldConfig(sub_document subdoc) const {
    if (_fields.empty()) {
        return;
    }
    subdoc.append(kvp("fields", [&](sub_array fieldsArray) {
        for (const auto& fieldPair : _fields) {
            fieldsArray.append(
                [&](sub_document fieldDoc) { fieldPair.second.appendEncryptInfo(fieldDoc); });
        }
    }));
}

void QueryableEncryptedCollection::createCollection(const mongocxx::client& client) const {
    auto createOpts = make_document(
        kvp("encryptedFields", [&](sub_document subdoc) { appendEncryptedFieldConfig(subdoc); }));
    BOOST_LOG_TRIVIAL(debug) << "Generated encryptedFieldConfig: "
                             << bsoncxx::to_json(createOpts.view());
    client[_database].create_collection(_collection, createOpts.view());
}

void QueryableEncryptedCollection::dropCollection(const mongocxx::client& client) const {
    // TODO: PERF-3306 remove this loop once CXX driver supports queryable-encryption
    for (auto& suffix : {".esc", ".ecc", ".ecoc"}) {
        std::string stateCollName = "enxcol_." + _collection + suffix;
        client[_database][stateCollName].drop();
    }
    client[_database][_collection].drop();
}

EncryptionOptions::EncryptionOptions(const Node& encryptionOptsNode) {
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
        throw InvalidConfigurationException(
            "'EncryptionOptions' requires an 'EncryptedCollections' node of sequence type");
    }
    auto collsVector = collsSequence.to<std::vector<std::string>>();
    _encryptedColls = std::unordered_set<std::string>(collsVector.begin(), collsVector.end());
}

class EncryptionManager::EncryptionManagerImpl {
public:
    // variant that holds either a FLE or Queryable encrypted collection
    using EncryptedCollectionVariant =
        std::variant<std::monostate, FLEEncryptedCollection, QueryableEncryptedCollection>;

    // maps a namespace string to an EncryptedCollectionVariant
    using EncryptedCollectionMap = std::unordered_map<std::string, EncryptedCollectionVariant>;

    // pair consisting of the key vault URI and the key vault namespace
    using KeyVaultID = std::pair<std::string, std::string>;

    EncryptionManagerImpl(const Node& yaml, bool dryRun = false);
    ~EncryptionManagerImpl();

    void createKeyVault(EncryptionContext& context, const std::string& uri);
    void createCollection(EncryptionContext& context,
                          const std::string& uri,
                          EncryptedCollectionVariant& collVariant);
    void createDataKeys(EncryptionContext& context,
                        const std::string& uri,
                        EncryptedCollectionVariant& collVariant);

    template <class CollectionType>
    void createDataKeysPerField(EncryptionContext& context,
                                mongocxx::client_encryption& clientEncryption,
                                CollectionType& encryptedColl,
                                const std::string& uri) {
        auto fieldsCopy = encryptedColl.fields();
        auto kvNs = context.getKeyVaultNamespaceString();
        for (auto& [fieldName, field] : fieldsCopy) {
            try {
                field.keyId(clientEncryption.create_data_key("local"));
                BOOST_LOG_TRIVIAL(debug)
                    << "Data key created on {" << kvNs << "@" << uri << "} for field '" << fieldName
                    << "' of " << encryptedColl.namespaceString() << ": "
                    << bsoncxx::to_json(make_document(kvp("keyId", field.keyId().value())));
            } catch (const mongocxx::exception& e) {
                BOOST_LOG_TRIVIAL(error)
                    << "Failed to create data key on {" << kvNs << "@" << uri << "}: " << e.what();
                throw e;
            }
        }
        encryptedColl.fields(std::move(fieldsCopy));
    }

    template <class CollectionType>
    void createCollection(EncryptionContext& context,
                          mongocxx::client& client,
                          CollectionType& encryptedColl,
                          const std::string& uri) {
        BOOST_LOG_TRIVIAL(debug) << "Dropping & creating encrypted collection '"
                                 << encryptedColl.namespaceString() << "' at " << uri;
        encryptedColl.dropCollection(client);
        encryptedColl.createCollection(client);
    }

private:
    friend class EncryptionContext;
    friend class EncryptionManager;

    // lock on modifying the key vaults map
    std::mutex _keyVaultsLock;

    // maps a KeyVaultID to a set of encrypted collections that have data keys in this key vault
    std::unordered_map<KeyVaultID, EncryptedCollectionMap, boost::hash<KeyVaultID>> _keyVaults;

    // stores the global set of encrypted collection schemas
    EncryptedCollectionMap _collectionSchemas;

    // whether setup that needs a server connection should be skipped
    bool _dryRun;

    // whether to use shared library instead of mongocryptd
    bool _useCryptSharedLib = false;

    // path to the mongo crypt shared library
    std::string _cryptSharedLibPath;
};

EncryptionManager::EncryptionManagerImpl::EncryptionManagerImpl(const Node& workloadCtx, bool dryRun)
    : _dryRun(dryRun) {
    const auto& encryptionNode = workloadCtx["Encryption"];
    if (!encryptionNode) {
        return;
    }

    _useCryptSharedLib = encryptionNode["UseCryptSharedLib"].maybe<bool>().value_or(false);

    if (_useCryptSharedLib) {
        _cryptSharedLibPath =
            encryptionNode["CryptSharedLibPath"].maybe<std::string>().value_or("");
        if (_cryptSharedLibPath.empty()) {
            throw InvalidConfigurationException(
                "A non-empty Encryption.CryptSharedLibPath is required if "
                "Encryption.UseCryptSharedLib is true");
        }
    }

    const auto& collsSequence = encryptionNode["EncryptedCollections"];
    if (!collsSequence) {
        return;
    }
    if (!collsSequence.isSequence()) {
        throw InvalidConfigurationException(
            "'Encryption.EncryptedCollections' node must be of sequence type");
    }

    for (const auto& [k, v] : collsSequence) {
        std::string nss;
        EncryptedCollectionVariant coll;
        auto typeStr = v["EncryptionType"].to<std::string>();

        if (typeStr == "fle") {
            auto& fleColl = coll.emplace<FLEEncryptedCollection>(v);
            nss = fleColl.namespaceString();
        } else if (typeStr == "queryable") {
            auto& qeColl = coll.emplace<QueryableEncryptedCollection>(v);
            nss = qeColl.namespaceString();
        } else {
            std::ostringstream ss;
            ss << "'EncryptedCollections." << k.toString()
               << "' has an invalid 'EncryptionType' value of '" << typeStr
               << "'. Valid values are 'fle' and 'queryable'.";
            throw InvalidConfigurationException(ss.str());
        }

        auto insertOk = _collectionSchemas.insert({nss, std::move(coll)}).second;
        if (!insertOk) {
            std::ostringstream ss;
            ss << "Collection with namespace '" << nss
               << "' already exists in 'EncryptedCollections'";
            throw InvalidConfigurationException(ss.str());
        }
    }
}

EncryptionManager::EncryptionManagerImpl::~EncryptionManagerImpl() {}

void EncryptionManager::EncryptionManagerImpl::createKeyVault(EncryptionContext& context,
                                                              const std::string& uri) {
    if (_dryRun) {
        return;
    }
    mongocxx::client client{mongocxx::uri(uri)};
    auto [keyVaultDb, keyVaultColl] = context.getKeyVaultNamespace();
    auto coll = client[keyVaultDb][keyVaultColl];

    BOOST_LOG_TRIVIAL(debug) << "Creating key vault at namespace "
                             << context.getKeyVaultNamespaceString();

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
}

void EncryptionManager::EncryptionManagerImpl::createCollection(
    EncryptionContext& context, const std::string& uri, EncryptedCollectionVariant& encryptedColl) {
    if (_dryRun) {
        return;
    }

    mongocxx::options::auto_encryption autoEncryptOpts;
    autoEncryptOpts.kms_providers(context.generateKMSProvidersDoc());
    autoEncryptOpts.key_vault_namespace(context.getKeyVaultNamespace());
    autoEncryptOpts.extra_options(context.generateExtraOptionsDoc());

    mongocxx::options::client clientOpts;
    clientOpts.auto_encryption_opts(std::move(autoEncryptOpts));

    mongocxx::client client{mongocxx::uri{uri}, std::move(clientOpts)};

    if (std::holds_alternative<FLEEncryptedCollection>(encryptedColl)) {
        createCollection(context, client, std::get<FLEEncryptedCollection>(encryptedColl), uri);
    }
    if (std::holds_alternative<QueryableEncryptedCollection>(encryptedColl)) {
        createCollection(
            context, client, std::get<QueryableEncryptedCollection>(encryptedColl), uri);
    }
}

void EncryptionManager::EncryptionManagerImpl::createDataKeys(
    EncryptionContext& context, const std::string& uri, EncryptedCollectionVariant& encryptedColl) {
    if (_dryRun) {
        return;
    }

    mongocxx::client client{mongocxx::uri(uri)};

    mongocxx::options::client_encryption clientEncryptionOpts;
    clientEncryptionOpts.key_vault_namespace(context.getKeyVaultNamespace());
    clientEncryptionOpts.kms_providers(context.generateKMSProvidersDoc());
    clientEncryptionOpts.key_vault_client(&client);
    mongocxx::client_encryption clientEncryption{std::move(clientEncryptionOpts)};

    // Create the data keys to be used for encrypting the values for each encrypted field in
    // the collection. Update the keyIds of each encrypted field with the UUID of the
    // newly-created data key.
    if (std::holds_alternative<FLEEncryptedCollection>(encryptedColl)) {
        createDataKeysPerField(
            context, clientEncryption, std::get<FLEEncryptedCollection>(encryptedColl), uri);
    }
    if (std::holds_alternative<QueryableEncryptedCollection>(encryptedColl)) {
        createDataKeysPerField(
            context, clientEncryption, std::get<QueryableEncryptedCollection>(encryptedColl), uri);
    }
}

EncryptionManager::EncryptionManager(const Node& workloadCtx, bool dryRun)
    : _impl(std::make_unique<EncryptionManagerImpl>(workloadCtx, dryRun)) {}

EncryptionManager::~EncryptionManager() {}

EncryptionContext EncryptionManager::createEncryptionContext(const std::string& uri,
                                                             const EncryptionOptions& opts) {
    std::lock_guard<std::mutex> kvLock{_impl->_keyVaultsLock};

    // Validate the collection names have a corresponding schema
    for (const auto& nss : opts.encryptedColls()) {
        if (_impl->_collectionSchemas.find(nss) == _impl->_collectionSchemas.end()) {
            std::ostringstream ss;
            ss << "No encrypted collection schema found with namespace '" << nss << "'";
            throw InvalidConfigurationException(ss.str());
        }
    }

    EncryptionContext context(opts, uri, *this);

    // Did we already create this key vault?
    auto kvId = std::make_pair(uri, context.getKeyVaultNamespaceString());
    auto kvItr = _impl->_keyVaults.find(kvId);
    if (kvItr == _impl->_keyVaults.end()) {
        // no, then create the key vault
        _impl->createKeyVault(context, uri);
        kvItr =
            _impl->_keyVaults.emplace(kvId, EncryptionManagerImpl::EncryptedCollectionMap()).first;
    }

    // then, for each namespace that does not yet have data keys in
    // this key vault, create the collection & its data keys.
    auto& collsInKeyVault = kvItr->second;
    for (const auto& nss : opts.encryptedColls()) {
        if (collsInKeyVault.find(nss) == collsInKeyVault.end()) {
            // get a copy of the schema
            auto collCopy = _impl->_collectionSchemas[nss];

            // create data keys and update the keyId fields with the generated data key IDs
            _impl->createDataKeys(context, uri, collCopy);
            collsInKeyVault[nss] = collCopy;

            // create the collection
            _impl->createCollection(context, uri, collCopy);
        }
    }

    return context;
}

EncryptionContext::EncryptionContext() {}

EncryptionContext::EncryptionContext(EncryptionOptions opts,
                                     std::string uri,
                                     const EncryptionManager& manager)
    : _encryptionOpts(std::move(opts)), _uri(std::move(uri)), _encryptionManager(&manager) {}

EncryptionContext::~EncryptionContext() {}

std::pair<std::string, std::string> EncryptionContext::getKeyVaultNamespace() const {
    return {_encryptionOpts.keyVaultDb(), _encryptionOpts.keyVaultColl()};
}

std::string EncryptionContext::getKeyVaultNamespaceString() const {
    return _encryptionOpts.keyVaultDb() + "." + _encryptionOpts.keyVaultColl();
}

mongocxx::options::auto_encryption EncryptionContext::getAutoEncryptionOptions() const {
    mongocxx::options::auto_encryption opts{};
    opts.key_vault_namespace(getKeyVaultNamespace());
    opts.kms_providers(generateKMSProvidersDoc());
    opts.schema_map(generateSchemaMapDoc());
    opts.extra_options(generateExtraOptionsDoc());
    return opts;
}

bsoncxx::document::value EncryptionContext::generateSchemaMapDoc() const {
    bsoncxx::builder::basic::document mapDoc{};

    if (!_encryptionManager) {
        return mapDoc.extract();
    }

    auto kvId = std::make_pair(_uri, getKeyVaultNamespaceString());
    auto kvItr = _encryptionManager->_impl->_keyVaults.find(kvId);
    if (kvItr == _encryptionManager->_impl->_keyVaults.end()) {
        BOOST_LOG_TRIVIAL(debug) << "No key vault found with ID: " << kvId.first << ", "
                                 << kvId.second;
        return mapDoc.extract();
    }

    auto& collsInKeyVault = kvItr->second;
    for (auto& nss : _encryptionOpts.encryptedColls()) {
        auto collItr = collsInKeyVault.find(nss);
        if (collItr == collsInKeyVault.end()) {
            continue;
        }
        auto& collVariant = collItr->second;
        if (std::holds_alternative<FLEEncryptedCollection>(collVariant)) {
            auto& fleColl = std::get<FLEEncryptedCollection>(collVariant);
            mapDoc.append(kvp(nss, [&](sub_document subdoc) { fleColl.appendSchema(subdoc); }));
        }
    }
    auto v = mapDoc.extract();
    BOOST_LOG_TRIVIAL(debug) << "Generated schema map: " << bsoncxx::to_json(v);
    return std::move(v);
}

bsoncxx::document::value EncryptionContext::generateEncryptedFieldsMapDoc() const {
    bsoncxx::builder::basic::document mapDoc{};

    auto kvId = std::make_pair(_uri, getKeyVaultNamespaceString());
    auto kvItr = _encryptionManager->_impl->_keyVaults.find(kvId);
    if (kvItr == _encryptionManager->_impl->_keyVaults.end()) {
        BOOST_LOG_TRIVIAL(debug) << "No key vault found with ID: " << kvId.first << ", "
                                 << kvId.second;
        return mapDoc.extract();
    }

    auto& collsInKeyVault = kvItr->second;
    for (auto& nss : _encryptionOpts.encryptedColls()) {
        auto collItr = collsInKeyVault.find(nss);
        if (collItr == collsInKeyVault.end()) {
            continue;
        }
        auto& collVariant = collItr->second;
        if (std::holds_alternative<QueryableEncryptedCollection>(collVariant)) {
            auto& qeColl = std::get<QueryableEncryptedCollection>(collVariant);
            mapDoc.append(
                kvp(nss, [&](sub_document subdoc) { qeColl.appendEncryptedFieldConfig(subdoc); }));
        }
    }
    auto v = mapDoc.extract();
    BOOST_LOG_TRIVIAL(debug) << "Generated encrypted fields map: " << bsoncxx::to_json(v);
    return std::move(v);
}

bsoncxx::document::value EncryptionContext::generateExtraOptionsDoc() const {
    bsoncxx::builder::basic::document extraOpts;

    // do not load shared library if this is a dry run
    bool shlibRequired = _encryptionManager && _encryptionManager->_impl->_useCryptSharedLib &&
        !_encryptionManager->_impl->_dryRun;

    extraOpts.append(kvp("mongocryptdBypassSpawn", true));
    extraOpts.append(kvp("cryptSharedLibRequired", shlibRequired));

    if (shlibRequired) {
        extraOpts.append(kvp("cryptSharedLibPath", _encryptionManager->_impl->_cryptSharedLibPath));
    }
    return extraOpts.extract();
}

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

bool EncryptionContext::hasEncryptedCollections() const {
    return !_encryptionOpts.encryptedColls().empty();
}

}  // namespace genny::v1
