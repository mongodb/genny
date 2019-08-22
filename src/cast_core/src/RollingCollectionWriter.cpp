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

#include <cast_core/actors/RollingCollectionWriter.hpp>
#include <cast_core/actors/RollingCollectionManager.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>
#include <boost/asio.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {
struct RollingCollectionWriter::PhaseConfig {
    mongocxx::database database;
    mongocxx::collection currentCollection;
    DocumentGenerator documentExpr;
    std::string nextCollectionName;
    bool haveCollection;
    metrics::Operation insertOperation;
    RollingCollectionWindow rollingCollectionWindow;
    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id, int64_t collectionWindowSize)
        : rollingCollectionWindow{collectionWindowSize}, database{db},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)},
          currentCollection{database["temp"]},
          haveCollection{false},
          insertOperation{phaseContext.operation("Insert", id)} {
              BOOST_LOG_TRIVIAL(info) << "Collection window size: " << collectionWindowSize;
          }
};

int64_t getCurrentSecond() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool updateCurrentIdWindow(RollingCollectionWindow& config){
    auto currentSecond = getCurrentSecond();
    if (currentSecond != config.lastSecond){
        if (config.lastSecond != INT64_MAX){
            int increase = currentSecond - config.lastSecond;
            BOOST_LOG_TRIVIAL(info) << "Current second: " << currentSecond << " lastSecond: " << config.lastSecond;
            config.max += increase;
            config.min += increase;
        }
        config.lastSecond = currentSecond;
        return true;
    }
    return false;
}

void RollingCollectionWriter::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (updateCurrentIdWindow(config->rollingCollectionWindow)){
                //Subtract 1 here as we've incremented it in the update window function.
                config->nextCollectionName = getRollingCollectionName(
                    config->rollingCollectionWindow.max - 1);
            }
            if (!config->database.has_collection(config->nextCollectionName)) {
                /*
                 * In this scenario except when we're on our first iteration we'll fall back to the
                 * previous collection.
                 */
                BOOST_LOG_TRIVIAL(debug) << "Missing collection: " << config->nextCollectionName;
            } else {
                config->currentCollection = config->database[config->nextCollectionName];
                config->haveCollection = true;
            }
            auto statTracker = config->insertOperation.start();
            if (config->haveCollection){
                auto document = config->documentExpr();
                config->currentCollection.insert_one(document.view());
                statTracker.addDocuments(1);
                statTracker.addBytes(document.view().length());
                statTracker.success();
            } else {
                statTracker.failure();
            }
        }
    }
}


void tick(const boost::system::error_code& /*e*/, boost::asio::deadline_timer *t,
      PhaseLoop<PhaseConfig>& loop) {

    std::cout << "tick" << std::endl;

    // Reschedule the timer for 1 second in the future:
    _timer.expires_at(_timer.expires_at() + boost::posix_time::seconds{1});
    // Posts the timer event
    _timer.async_wait(tick);
}

RollingCollectionWriter::RollingCollectionWriter(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _collectionWindowSize{context["CollectionWindowSize"].maybe<IntegerSpec>().value_or(0)},
      _loop{context,
            (*_client)[context["Database"].to<std::string>()],
            RollingCollectionWriter::id(),
            context["CollectionWindowSize"].maybe<IntegerSpec>().value_or(0)} {
            boost::asio::io_service io_service;
            boost::posix_time::seconds interval(1);
            _timer{io_service, interal};
            _timer.async_wait(boost::bind(tick, boost::asoi::placeholders::error, _timer, _loop));
            io_service.run();
        }

namespace {
auto registerRollingCollectionWriter = Cast::registerDefault<RollingCollectionWriter>();
}  // namespace
}  // namespace genny::actor
