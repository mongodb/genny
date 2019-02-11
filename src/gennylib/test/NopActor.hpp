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

#ifndef HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED
#define HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED

#include <gennylib/Actor.hpp>
#include <gennylib/ActorProducer.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

class NopActor : public Actor {
public:
    explicit NopActor(ActorContext& c) : Actor(c), _loop{c} {}

    void run() override {
        for (auto&& p : _loop) {
            for (auto&& _ : p) {
                // nop
            }
        }
    }

    static std::string_view defaultName() {
        return "NopActor";
    }

    static std::shared_ptr<ActorProducer> producer() {
        static std::shared_ptr<ActorProducer> _producer =
            std::make_shared<genny::DefaultActorProducer<genny::actor::NopActor>>();
        return _producer;
    }

private:
    struct PhaseConfig {
        explicit PhaseConfig(PhaseContext&) {}
    };
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_ED5EA6B0_E1FF_4921_80BB_689D6C710720_INCLUDED
