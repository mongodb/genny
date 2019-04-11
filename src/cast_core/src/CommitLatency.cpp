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

#include <cast_core/actors/CommitLatency.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/random.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/context.hpp>


namespace genny::actor {

struct CommitLatency::PhaseConfig {
    mongocxx::collection collection;
    mongocxx::write_concern wc;
    mongocxx::read_concern rc;
    mongocxx::read_preference rp;
    std::string readModeString;
    std::shared_ptr<mongocxx::client_session> session;
    mongocxx::options::find optionsFind;
    mongocxx::options::update optionsUpdate;
    mongocxx::options::aggregate optionsAggregate;
    mongocxx::options::transaction optionsTransaction;
    bool useSession;
    bool useTransaction;
    int64_t repeat;
    int64_t threads;
    boost::random::uniform_int_distribution<int64_t> amountDistribution;
    ExecutionStrategy::RunOptions options;

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          wc{phaseContext.get<mongocxx::write_concern>("WriteConcern")},
          rc{phaseContext.get<mongocxx::read_concern>("ReadConcern")},
          rp{phaseContext.get<mongocxx::read_preference>("ReadPreference")},
          readModeString{phaseContext.get("ReadPreference")["ReadMode"].as<std::string>()},
          useSession{phaseContext.get<bool, false>("Session").value_or(false)},
          useTransaction{phaseContext.get<bool, false>("Transaction").value_or(false)},
          repeat{phaseContext.get<IntegerSpec>("Repeat")},
          threads{phaseContext.get<IntegerSpec>("Threads")},
          amountDistribution{-100, 100},
          options{phaseContext.get<ExecutionStrategy::RunOptions,false>("ExecutionsStrategy").value_or(ExecutionStrategy::RunOptions{})} {
        if (useTransaction) {
            optionsTransaction.write_concern(wc);
            optionsTransaction.read_preference(rp);
            // Transactions ignore the read_concern of a collection handle, must set transaction
            // options.
            optionsTransaction.read_concern(rc);
        } else {
            optionsUpdate.write_concern(wc);
            optionsFind.read_preference(rp);
            optionsAggregate.read_preference(rp);
            // Ugh... read_concern cannot be set for operations individually, only through client,
            // database, or collection. (CXX-1748)
            collection.read_concern(rc);
        }
    }
};

void CommitLatency::run() {
    using namespace bsoncxx::builder::stream;

    for (auto&& config : _loop) {

        std::shared_ptr<mongocxx::client_session> _session;
        if (config.begin() != config.end()) {
            BOOST_LOG_TRIVIAL(info)
                << "Starting " << config->threads << "x" << config->repeat
                << " CommitLatency transactions (2 finds, 2 updates, 1 aggregate).";
            BOOST_LOG_TRIVIAL(info)
                << "CommitLatency options: wc=" << bsoncxx::to_json(config->wc.to_document())
                << " rc=" << config->rc.acknowledge_string() << " rp=" << config->readModeString
                << " session=" << (config->useSession ? "true" : "false")
                << " transaction=" << (config->useTransaction ? "true" : "false");


            // start_session requires access to _client object, and session objects are of course
            // per thread, so we initialize it here.
            if (config->useSession || config->useTransaction) {
                // Sessions have causal consistency by default.
                // It's also possible to unset that, but we don't provide a yaml option to actually
                // do that.
                _session = std::make_shared<mongocxx::client_session>(_client->start_session());
            }
        }
        for (const auto&& _ : config) {
            _strategy.run(
                [&](metrics::OperationContext& ctx) {
                    // Basically we withdraw `amount` from account 1 and deposit to account 2
                    // amount = random.randint(-100, 100)
                    auto amount = config->amountDistribution(_rng);

                    if (config->useTransaction) {
                        _session->start_transaction(config->optionsTransaction);
                    }

                    // result1 = db.hltest.find_one( { '_id': 1 }, session=session )
                    bsoncxx::document::value doc_filter1 = document{} << "_id" << 1 << finalize;
                    auto result1 = config->collection.find_one(
                        *_session, doc_filter1.view(), config->optionsFind);

                    // db.hltest.update_one( {'_id': 1}, {'$inc': {'n': -amount}}, session=session )
                    bsoncxx::document::value doc_update1 = document{}
                        << "$inc" << open_document << "n" << -amount << close_document << finalize;
                    config->collection.update_one(
                        *_session, doc_filter1.view(), doc_update1.view(), config->optionsUpdate);

                    // result2 = db.hltest.find_one( { '_id': 2 }, session=session )
                    bsoncxx::document::value doc_filter2 = document{} << "_id" << 2 << finalize;
                    auto result2 = config->collection.find_one(
                        *_session, doc_filter2.view(), config->optionsFind);

                    // db.hltest.update_one( {'_id': 2}, {'$inc': {'n': amount}}, session=session )
                    bsoncxx::document::value doc_update2 = document{}
                        << "$inc" << open_document << "n" << amount << close_document << finalize;
                    config->collection.update_one(
                        *_session, doc_filter2.view(), doc_update2.view(), config->optionsUpdate);

                    // result = db.hltest.aggregate( [ { '$group': { '_id': 'foo', 'total' : {
                    // '$sum': '$n' } } } ], session=session ).next()
                    mongocxx::pipeline p{};
                    bsoncxx::document::value doc_group = document{}
                        << "_id"
                        << "foo"
                        << "total" << open_document << "$sum"
                        << "$n" << close_document << finalize;
                    p.group(doc_group.view());
                    auto cursor =
                        config->collection.aggregate(*_session, p, config->optionsAggregate);
                    // Check for isolation or consistency errors.
                    //
                    // These are expected when read_preference is secondary and not in a session.
                    // Also could happen on a primary if using multiple threads.
                    //
                    // Note: If there's skew in the sum total, we never fix it. So after the first
                    // error, this counter is likely to increment on every loop after that. The
                    // exact value of the metric is therefore uninteresting, it's only significant
                    // whether it is zero or non-zero.
                    for (auto doc : cursor) {
                        if (doc["total"].get_int64() != 200) {
                            ctx.addErrors(1);
                        }
                    }

                    if (config->useTransaction) {
                        _session->commit_transaction();
                    }
                },
                config->options);
        }
    }
}

CommitLatency::CommitLatency(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().getRNGForThread(CommitLatency::id())},
      _strategy{context.operation("insert", CommitLatency::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerCommitLatency = Cast::registerDefault<CommitLatency>();
}  // namespace
}  // namespace genny::actor
