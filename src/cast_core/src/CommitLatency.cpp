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

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/ExecutionStrategy.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/context.hpp>
#include <value_generators/value_generators.hpp>


namespace genny::actor {

struct CommitLatency::PhaseConfig {
    mongocxx::collection collection;
    mongocxx::write_concern wc;
    mongocxx::read_concern rc;
    mongocxx::read_preference rp;
    std::string rp_string;
    std::shared_ptr<mongocxx::client_session> session;
    mongocxx::options::find optionsFind;
    mongocxx::options::update optionsUpdate;
    mongocxx::options::aggregate optionsAggregate;
    bool useSession;
    int64_t repeat;
    int64_t threads;
    std::uniform_int_distribution<int64_t> amountDistribution;
    ExecutionStrategy::RunOptions options;

    PhaseConfig(PhaseContext& phaseContext, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          rp_string{phaseContext.get<std::string>("ReadPreference")},
          useSession{phaseContext.get<bool, false>("Session")},
          repeat{phaseContext.get<IntegerSpec>("Repeat")},
          threads{phaseContext.get<IntegerSpec>("Threads")},
          amountDistribution{-100, 100},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {
              // write_concern
              if( phaseContext.get<bool, false>("WriteConcernMajority") ) {
                  wc.majority(std::chrono::milliseconds{0});
              }
              else {
                  wc.nodes( (std::int32_t) phaseContext.get<IntegerSpec, false>("WriteConcern").value_or(1) );
              }
              if( phaseContext.get<bool, false>("WriteConcernJournal") ) {
                  wc.journal(true);
              }
              if( ! phaseContext.get<bool, false>("WriteConcernJournal").value_or(true) ) {
                  // On some mongod versions it makes a difference whether journal is unset vs explicitly set to false
                  wc.journal(false);
              }
              optionsUpdate.write_concern(wc);

              // read_concern
              rc.acknowledge_string(phaseContext.get<std::string>("ReadConcern"));
              // Ugh... read_concern cannot be set for operations individually, only through client, database, or collection.
              // And out of those, only client is valid inside transactions. (DRIVERS-619)
              // It seems mongocxx::database doesn't provide a getter to get the client, nor is the client available in this context.
              // Ergo, transactions cannot be supported...
              // TODO: Should set _client. (And options::find etc should support read_concern!)
              collection.read_concern(rc);

              // read_preference
              if ( rp_string == "PRIMARY" ) {
                  // TODO: This actually sends primaryPreferred to server. -> File bug in Jira.
                  // This is benign for this test, but potentially bad for real apps!
                  // Note that k_secondary works correctly.
                  rp.mode(mongocxx::read_preference::read_mode::k_primary);
              }
              else if ( rp_string == "SECONDARY" ) {
                  rp.mode(mongocxx::read_preference::read_mode::k_secondary);
              }
              else {
                  throw InvalidConfigurationException("ReadPreference must be PRIMARY or SECONDARY.");
              }
              optionsFind.read_preference(rp);
              optionsAggregate.read_preference(rp);

              // client_session
              if( useSession ) {
                  // TODO: Also this requires access to _client object
              }
          }
};

void CommitLatency::run() {
    for (auto&& config : _loop) {
        if (config.begin() != config.end()) {
              BOOST_LOG_TRIVIAL(info) << "Starting " << config->threads << "x" << config->repeat 
                                      << " CommitLatency transactions (2 finds, 2 updates, 1 aggregate).";
              BOOST_LOG_TRIVIAL(info) << "CommitLatency options: wc=" 
                                      << bsoncxx::to_json(config->wc.to_document())
                                      << " rc="
                                      << config->rc.acknowledge_string()
                                      << " rp="
                                      << config->rp_string
                                      << " session=false transaction=false";
        }
        for (const auto&& _ : config) {
            _strategy.run(
                [&](metrics::OperationContext& ctx) {
                    // Basically we withdraw `amount` from account 1 and deposit to account 2
                    // amount = random.randint(-100, 100)
                    auto amount = config->amountDistribution(_rng);

                    // result1 = db.hltest.find_one( { '_id': 1 }, session=session )
                    bsoncxx::document::value doc_filter1 = bsoncxx::builder::stream::document{}
                        << "_id" << 1
                        << bsoncxx::builder::stream::finalize;
                    auto result1 = config->collection.find_one(doc_filter1.view(), config->optionsFind);

                    // db.hltest.update_one( {'_id': 1}, {'$inc': {'n': -amount}}, session=session )
                    bsoncxx::document::value doc_update1 = bsoncxx::builder::stream::document{}
                        << "$inc" << bsoncxx::builder::stream::open_document
                            << "n" << -amount
                        << bsoncxx::builder::stream::close_document
                        << bsoncxx::builder::stream::finalize;
                    config->collection.update_one(doc_filter1.view(), doc_update1.view(),
                                                  config->optionsUpdate);

                    // result2 = db.hltest.find_one( { '_id': 2 }, session=session )
                    bsoncxx::document::value doc_filter2 = bsoncxx::builder::stream::document{}
                        << "_id" << 2
                        << bsoncxx::builder::stream::finalize;
                    auto result2 = config->collection.find_one(doc_filter2.view(), config->optionsFind);

                    // db.hltest.update_one( {'_id': 2}, {'$inc': {'n': amount}}, session=session )
                    bsoncxx::document::value doc_update2 = bsoncxx::builder::stream::document{}
                        << "$inc" << bsoncxx::builder::stream::open_document
                            << "n" << amount
                        << bsoncxx::builder::stream::close_document
                        << bsoncxx::builder::stream::finalize;
                    config->collection.update_one(doc_filter2.view(), doc_update2.view(),
                                                  config->optionsUpdate);

                    // result = db.hltest.aggregate( [ { '$group': { '_id': 'foo', 'total' : { '$sum': '$n' } } } ], session=session ).next()
                    mongocxx::pipeline p{};
                    bsoncxx::document::value doc_group = bsoncxx::builder::stream::document{}
                            << "_id" << "foo"
                            << "total" << bsoncxx::builder::stream::open_document
                                << "$sum" << "$n"
                            << bsoncxx::builder::stream::close_document
                        << bsoncxx::builder::stream::finalize;
                    p.group(doc_group.view());
                    auto cursor = config->collection.aggregate(p, config->optionsAggregate);
                    // Check for isolation or consistency errors
                    // Note: If there's skew in the sum total, we never fix it. So after the first
                    // error, this counter is likely to increment on every loop after that. The
                    // exact value of the metric is therefore uninteresting, it's only significant
                    // whether it is zero or non-zero.
                    for (auto doc : cursor) {
                        if (doc["total"].get_int64() != 200) {
                            ctx.addErrors(1);
                        }
                    }
                },
                config->options);
        }
    }
}

CommitLatency::CommitLatency(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _strategy{context.operation("insert", CommitLatency::id())},
      _client{std::move(context.client())},
      _loop{context, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerCommitLatency = Cast::registerDefault<CommitLatency>();
}  // namespace
}  // namespace genny::actor
