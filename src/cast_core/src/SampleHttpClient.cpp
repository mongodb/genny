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


#include <cast_core/http.hpp>
#include <cast_core/actors/SampleHttpClient.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

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

            auto requests = _totalHttpRequests.start();

            BOOST_LOG_TRIVIAL(debug) << " SampleHttpClient Inserting "
                                    << bsoncxx::to_json(document.view());


            try {
                auto host = "google.com";
                auto port = "443";
                auto stream = http::make_tls_stream("google.com", "443");

                // Set up an HTTP GET request message
                boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::get, "/", 11};
                req.set(boost::beast::http::field::host, host);
                // Send the HTTP request to the remote host
                boost::beast::http::write(stream, req);

                // This buffer is used for reading and must be persisted
                boost::beast::flat_buffer buffer;

                // Declare a container to hold the response
                boost::beast::http::response<boost::beast::http::dynamic_body> res;

                // Receive the HTTP response
                boost::beast::http::read(stream, buffer, res);

                // Write the message to standard out
                std::cout << res << std::endl;

                // Gracefully close the stream
                boost::beast::error_code ec;
                stream.shutdown(ec);

                if(ec) {
                    requests.failure();
                } else {
                    requests.success();
                }

            } catch(...) {
                requests.failure();
                throw std::current_exception();
            }
        }
    }
}

SampleHttpClient::SampleHttpClient(genny::ActorContext& context)
    : Actor{context},
      _totalHttpRequests{context.operation("Request", SampleHttpClient::id())},
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
//
auto registerSampleHttpClient = Cast::registerDefault<SampleHttpClient>();
}  // namespace
}  // namespace genny::actor
