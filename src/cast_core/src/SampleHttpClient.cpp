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

#include <cast_core/actors/SampleHttpClient.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

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
#include <simple-beast-client/httpclient.hpp>


namespace genny::actor {

struct SampleHttpClient::PhaseConfig {
    mongocxx::database database;

    mongocxx::collection collection;

    DocumentGenerator documentExpr;

    PhaseConfig(PhaseContext& phaseContext, mongocxx::database&& db, ActorId id)
        : database{db},
          collection{database[phaseContext["Collection"].to<std::string>()]},
          documentExpr{phaseContext["Document"].to<DocumentGenerator>(phaseContext, id)} {}
};


//
// Genny spins up a thread for each Actor instance. The `Threads:` configuration
// tells Genny how many such instances to create. See further documentation in
// the `Actor.hpp` file.
//
void SampleHttpClient::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto document = config->documentExpr();

            auto requests = _totalRequests.start();

            BOOST_LOG_TRIVIAL(debug) << " SampleHttpClient Inserting "
                                    << bsoncxx::to_json(document.view());

            try {
                boost::asio::io_context ioContext;
                auto client = std::make_shared<simple_http::get_client>(
                    ioContext,
                    [](simple_http::empty_body_request& /*req*/,
                    simple_http::string_body_response& resp) {
                    // noop for successful HTTP
                    });

                client->setFailHandler([&requests](const simple_http::empty_body_request& /*req*/,
                            const simple_http::string_body_response& /*resp*/,
                            simple_http::fail_reason fr, boost::string_view message) {
                        std::cout << "Failure code: " << fr << '\n';
                    requests.failure();
                });

                // Run the GET request to httpbin.org
                client->get(simple_http::url{
                    "https://user:passwd@httpbin.org/digest-auth/auth/user/passwd/MD5/never"});

                ioContext.run();

                requests.success();
            } catch(mongocxx::operation_exception& e) {
                requests.failure();
                //
                // MongoException lets you include a "causing" bson document in the
                // exception message for help debugging.
                //
                BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            } catch(...) {
                requests.failure();
                throw std::current_exception();
            }
        }
    }
}

SampleHttpClient::SampleHttpClient(genny::ActorContext& context)
    : Actor{context},
      _totalRequests{context.operation("Insert", SampleHttpClient::id())},
      _client{context.client()},
      //
      // Pass any additional constructor parameters that your `PhaseConfig` needs.
      //
      // The first argument passed in here is actually the `ActorContext` but the
      // `PhaseLoop` reads the `PhaseContext`s from there and constructs one
      // instance for each Phase.
      //
      _loop{context, (*_client)[context["Database"].to<std::string>()], SampleHttpClient::id()} {}

namespace {
//
// This tells the "global" cast about our actor using the defaultName() method
// in the header file.
auto registerSampleHttpClient = Cast::registerDefault<SampleHttpClient>();
}  // namespace
}  // namespace genny::actor
