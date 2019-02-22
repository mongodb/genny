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

#include <cast_core/actors/HelloWorld.hpp>

#include <string>

#include <boost/log/trivial.hpp>

#include <gennylib/Cast.hpp>

namespace genny::actor {

/** @private */
struct HelloWorld::PhaseConfig {
    std::string message;
    explicit PhaseConfig(PhaseContext& context)
        : message{context.get<std::string, false>("Message").value_or("Hello, World!")} {}
};

void HelloWorld::run() {
    for (auto&& config : _loop) {
        for (auto _ : config) {
            auto ctx = this->_operation.start();
            BOOST_LOG_TRIVIAL(info) << config->message;
            ++_helloCounter;
            BOOST_LOG_TRIVIAL(info) << "Counter: " << _helloCounter;
            ctx.addDocuments(1);
            ctx.addBytes(config->message.size());
            ctx.success();
        }
    }
}

HelloWorld::HelloWorld(genny::ActorContext& context)
    : Actor(context),
      _operation{context.operation("Op", HelloWorld::id())},
      _helloCounter{WorkloadContext::getActorSharedState<HelloWorld, HelloWorldCounter>()},
      _loop{context} {}

namespace {
auto registerHelloWorld = genny::Cast::registerDefault<genny::actor::HelloWorld>();
}
}  // namespace genny::actor
