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

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <simple-beast-client/httpclient.hpp>
#include <value_generators/DocumentGenerator.hpp>


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


void SampleHttpClient::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto document = config->documentExpr();

            auto requests = _totalRequests.start();

            BOOST_LOG_TRIVIAL(debug)
                << " SampleHttpClient Inserting " << bsoncxx::to_json(document.view());

            try {
                boost::asio::io_context ioContext;
                auto client = std::make_shared<simple_http::get_client>(
                    ioContext,
                    [](simple_http::empty_body_request& /*req*/,
                       simple_http::string_body_response& resp) {
                        // noop for successful HTTP
                    });

                client->setFailHandler(
                    [&requests](const simple_http::empty_body_request& /*req*/,
                                const simple_http::string_body_response& /*resp*/,
                                simple_http::fail_reason fr,
                                boost::string_view message) {
                        // TODO TIG-3843: this will always be triggered on macOS
                        // with Failure code 2, Message: Error resolving
                        // target: Host not found (authoritative)
                        BOOST_LOG_TRIVIAL(warning) << "Failure code: " << fr << std::endl;
                        BOOST_LOG_TRIVIAL(warning) << "Message: " << message << std::endl;
                        requests.failure();
                    });

                // Run the GET request to httpbin.org
                client->get(simple_http::url{
                    "https://user:passwd@httpbin.org/digest-auth/auth/user/passwd/MD5/never"});

                ioContext.run();  // blocks until requests are complete.

                requests.success();
            } catch (mongocxx::operation_exception& e) {
                requests.failure();
                BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            } catch (...) {
                requests.failure();
                throw;
            }
        }
    }
}

SampleHttpClient::SampleHttpClient(genny::ActorContext& context)
    : Actor{context},
      _totalRequests{context.operation("Insert", SampleHttpClient::id())},
      _client{context.client()},
      _loop{context, (*_client)[context["Database"].to<std::string>()], SampleHttpClient::id()} {}

namespace {
auto registerSampleHttpClient = Cast::registerDefault<SampleHttpClient>();
}  // namespace
}  // namespace genny::actor
