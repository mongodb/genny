#include <gennylib/actors/Loader.hpp>

#include <memory>

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <yaml-cpp/yaml.h>

#include "log.hh"
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>


namespace {

}  // namespace

struct genny::actor::Loader::PhaseConfig {
    PhaseConfig(PhaseContext& context, std::mt19937_64& rng, mongocxx::pool::entry& client)
        : database{(*client)[context.get<std::string>("Database")]},
          numCollections{context.get<uint>("CollectionCount")},
          numDocuments{context.get<uint>("DocumentCount")},
          documentTemplate{value_generators::makeDoc(context.get("Document"), rng)} {
              // how do I parse an initialize the indexes?
          }

    mongocxx::database database;
    uint numCollections;
    uint numDocuments;
    std::unique_ptr<value_generators::DocumentGenerator> documentTemplate;
    std::vector<std::unique_ptr<value_generators::DocumentGenerator>> indexes;
    
};

void genny::actor::Loader::run() {
    for (auto&& [phase, config] : _loop) {
        if (phase == 0) {
            // Only operates in phase 0. This should be generalized. 
            // TODO: main logic
            // For each collection
            config->database.drop();
            for (uint i = 0; i < config->numCollections; i++) {
                auto collectionName = "Collection" + std::to_string(i);
                auto collection = config->database[collectionName];
                // Insert the documents
                // TODO: Add a batch size argument and use bulk writes
                for (uint j = 0; j < config->numDocuments; j++) {
                    bsoncxx::builder::stream::document doc;
                    auto docView = config->documentTemplate->view(doc);
                    collection.insert_one(docView);
                }
                // For each index
                for (auto& index : config->indexes) {
                    // Make the index
                    bsoncxx::builder::stream::document keys;
                    auto keyView = index->view(keys);
                    collection.create_index(keyView);
                }
            }
        }
    }
}

genny::actor::Loader::Loader(genny::ActorContext& context, const unsigned int thread)
    : _rng{context.workload().createRNG()},
      _client{std::move(context.client())},
      _loop{context, _rng, _client} {}

genny::ActorVector genny::actor::Loader::producer(genny::ActorContext& context) {
    auto out = std::vector<std::unique_ptr<genny::Actor>>{};
    if (context.get<std::string>("Type") != "Loader") {
        return out;
    }
    // Loader is single threaded for now
    out.push_back(std::make_unique<genny::actor::Loader>(context, 0));
    return out;
}
