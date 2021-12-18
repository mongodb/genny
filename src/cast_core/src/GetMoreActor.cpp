// Copyright 2021-present MongoDB Inc.
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

#include <cast_core/actors/GetMoreActor.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

struct GetMoreActor::PhaseConfig {
    PhaseConfig(PhaseContext& phaseContext, mongocxx::pool::entry& client, ActorId id);

    mongocxx::database db;
    DocumentGenerator initialCommandExpr;
    int64_t batchSize;
};

GetMoreActor::PhaseConfig::PhaseConfig(PhaseContext& phaseContext,
                                       mongocxx::pool::entry& client,
                                       ActorId id)
    : db{client->database(phaseContext["Database"].to<std::string>())},
      batchSize{phaseContext["GetMoreBatchSize"].to<IntegerSpec>()},
      initialCommandExpr{
          phaseContext["InitialCursorCommand"].to<DocumentGenerator>(phaseContext, id)} {}

void GetMoreActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            bsoncxx::document::view_or_value initialCursorCmd;
            bsoncxx::stdx::string_view collectionName;
            int64_t cursorId = 0LL;

            auto overallCursorCtx = _overallCursor.start();

            {
                auto initialCmdCtx = _initialRequest.start();
                initialCursorCmd = config->initialCommandExpr();
                collectionName = initialCursorCmd.view().cbegin()->get_utf8().value;

                cursorId = _runCursorCommand(config->db,
                                             initialCursorCmd.view(),
                                             "firstBatch",
                                             initialCmdCtx,
                                             overallCursorCtx);

                initialCmdCtx.success();
            }

            auto getMoreCmd = bsoncxx::builder::basic::make_document(
                bsoncxx::builder::basic::kvp("getMore", cursorId),
                bsoncxx::builder::basic::kvp("collection", collectionName));

            while (cursorId != 0LL) {
                auto getMoreCmdCtx = _individualGetMore.start();

                cursorId = _runCursorCommand(
                    config->db, getMoreCmd.view(), "nextBatch", getMoreCmdCtx, overallCursorCtx);

                getMoreCmdCtx.success();
            }

            overallCursorCtx.success();
        }
    }
}

int64_t GetMoreActor::_runCursorCommand(mongocxx::database& db,
                                        bsoncxx::document::view cmdRequest,
                                        bsoncxx::stdx::string_view cursorResultsField,
                                        metrics::OperationContext& requestMetricsCtx,
                                        metrics::OperationContext& overallMetricsCtx) {
    bsoncxx::document::view_or_value response;
    bsoncxx::document::view cursorResponse;
    try {
        response = db.run_command(cmdRequest);
    } catch (const mongocxx::operation_exception& ex) {
        requestMetricsCtx.failure();
        overallMetricsCtx.failure();
        BOOST_THROW_EXCEPTION(MongoException(ex, cmdRequest));
    }

    cursorResponse = response.view()["cursor"].get_document();
    auto cursorId = cursorResponse["id"].get_int64();

    // TODO: Update stats.
    // cursorResponse[cursorResultsField].get_array().value;

    return cursorId;
}

GetMoreActor::GetMoreActor(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _overallCursor{context.operation("OverallCursor", GetMoreActor::id())},
      _initialRequest{context.operation("InitialRequest", GetMoreActor::id())},
      _individualGetMore{context.operation("IndividualGetMore", GetMoreActor::id())},
      _loop{context, _client, GetMoreActor::id()} {}

namespace {
auto registerGetMoreActor = Cast::registerDefault<GetMoreActor>();
}  // namespace
}  // namespace genny::actor
