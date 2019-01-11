#include <cast_core/actors/Loader.hpp>

#include <algorithm>
#include <memory>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::actor {

/** @private */
using document_ptr = std::unique_ptr<genny::value_generators::DocumentGenerator>;

/** @private */
using index_type = std::pair<document_ptr, std::optional<document_ptr>>;

/** @private */
struct Loader::PhaseConfig {
    PhaseConfig(PhaseContext& context,
                genny::DefaultRandom& rng,
                mongocxx::pool::entry& client,
                uint thread)
        : database{(*client)[context.get<std::string>("Database")]},
          // The next line uses integer division. The Remainder is accounted for below.
          numCollections{context.get<UIntSpec, true>("CollectionCount") /
                         context.get<UIntSpec, true>("Threads")},
          numDocuments{context.get<UIntSpec, true>("DocumentCount")},
          batchSize{context.get<UIntSpec, true>("BatchSize")},
          documentTemplate{value_generators::makeDoc(context.get("Document"), rng)},
          collectionOffset{numCollections * thread} {
        auto indexNodes = context.get("Indexes");
        for (auto indexNode : indexNodes) {
            indexes.emplace_back(
                value_generators::makeDoc(indexNode["keys"], rng),
                indexNode["options"]
                    ? std::make_optional(value_generators::makeDoc(indexNode["options"], rng))
                    : std::nullopt);
        }
        if (thread == context.get<int>("Threads") - 1) {
            // Pick up any extra collections left over by the division
            numCollections += context.get<uint>("CollectionCount") % context.get<uint>("Threads");
        }
    }

    mongocxx::database database;
    uint64_t numCollections;
    uint64_t numDocuments;
    uint64_t batchSize;
    document_ptr documentTemplate;
    std::vector<index_type> indexes;
    uint64_t collectionOffset;
};

void genny::actor::Loader::run() {
    for (auto&& config : _loop) {
        for (auto&& _ : config) {
            for (uint i = config->collectionOffset;
                 i < config->collectionOffset + config->numCollections;
                 i++) {
                auto collectionName = "Collection" + std::to_string(i);
                auto collection = config->database[collectionName];
                // Insert the documents
                uint remainingInserts = config->numDocuments;
                {
                    auto totalOp = _totalBulkLoadTimer.raii();
                    while (remainingInserts > 0) {
                        // insert the next batch
                        uint numberToInsert =
                            std::min<uint64_t>(config->batchSize, remainingInserts);
                        std::vector<bsoncxx::builder::stream::document> docs(numberToInsert);
                        std::vector<bsoncxx::document::view> views;
                        auto newDoc = docs.begin();
                        for (uint j = 0; j < numberToInsert; j++, newDoc++) {
                            views.push_back(config->documentTemplate->view(*newDoc));
                        }
                        {
                            auto individualOp = _individualBulkLoadTimer.raii();
                            auto result = collection.insert_many(views);
                            remainingInserts -= result->inserted_count();
                        }
                    }
                }
                // For each index
                for (auto&& [keys, options] : config->indexes) {
                    // Make the index
                    bsoncxx::builder::stream::document keysDoc;
                    bsoncxx::builder::stream::document optionsDoc;
                    auto keyView = keys->view(keysDoc);
                    BOOST_LOG_TRIVIAL(debug) << "Building index " << bsoncxx::to_json(keyView);
                    if (options) {
                        auto optionsView = (*options)->view(optionsDoc);
                        BOOST_LOG_TRIVIAL(debug)
                            << "With options " << bsoncxx::to_json(optionsView);
                        auto op = _indexBuildTimer.raii();
                        collection.create_index(keyView, optionsView);
                    } else {
                        auto op = _indexBuildTimer.raii();
                        collection.create_index(keyView);
                    }
                }
            }
            BOOST_LOG_TRIVIAL(info) << "Done with load phase. All documents loaded";
        }
    }
}

Loader::Loader(genny::ActorContext& context, uint thread)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _totalBulkLoadTimer{context.timer("totalBulkInsertTime", Loader::id())},
      _individualBulkLoadTimer{context.timer("individualBulkInsertTime", Loader::id())},
      _indexBuildTimer{context.timer("indexBuildTime", Loader::id())},
      _client{context.client()},
      _loop{context, _rng, _client, thread} {}

class LoaderProducer : public genny::ActorProducer {
public:
    LoaderProducer(const std::string_view& name) : ActorProducer(name) {}
    genny::ActorVector produce(genny::ActorContext& context) {
        if (context.get<std::string>("Type") != "Loader") {
            return {};
        }
        genny::ActorVector out;
        for (uint i = 0; i < context.get<int>("Threads"); ++i) {
            out.emplace_back(std::make_unique<genny::actor::Loader>(context, i));
        }
        return out;
    }
};

namespace {
std::shared_ptr<genny::ActorProducer> loaderProducer = std::make_shared<LoaderProducer>("Loader");
auto registration = genny::Cast::registerCustom<genny::ActorProducer>(loaderProducer);
}  // namespace
}  // namespace genny::actor
