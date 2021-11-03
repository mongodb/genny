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

#ifndef HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
#define HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED

#include <memory>
#include <string_view>

#include <gennylib/ActorVector.hpp>
#include <gennylib/Actor.hpp>

namespace genny {

class ActorContext;

/**
 * `ActorProducer.produce()` maps from ActorContext -> vector of `Actor`s.
 *
 * For the following %YAML,
 *
 * ```yaml
 * SchemaVersion: 2018-07-01
 * Actors:
 * - Name: Foo
 * - Name: Bar
 * ```
 *
 * each ActorProducer will be called twice: once with the ActorContext for
 * `{Name: Foo}` and another with the ActorContext for `{Name: Bar}`.
 *
 * Many `ActorProducer`s will want to return an empty ActorVector if the
 * "Name" field is different from what they expect, but this is just
 * a convention.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call `ActorContext::get()` only during their constructors and retain
 * refs or copies of config objects
 */
class ActorProducer {
public:
    explicit ActorProducer(std::string_view name) : _name{name} {}

    ActorProducer(const ActorProducer&) = delete;
    ActorProducer& operator=(const ActorProducer&) = delete;
    ActorProducer(ActorProducer&&) = default;
    ActorProducer& operator=(ActorProducer&&) = default;

    constexpr const std::string_view& name() const {
        return _name;
    }

    /**
     * Called in the single thread prior to parallel construction to
     * claim the actor IDs this producer is responsible for and store
     * the context.
     */
    virtual void claimActorContext(ActorContext& context) = 0;

    /**
     * Produce using the claimed context.
     */
    virtual ActorVector produce() = 0;

private:
    std::string_view _name;
};

/** @private */
class ParallelizedActorProducer : public ActorProducer {
public:
    using ActorProducer::ActorProducer;

    virtual void produceInto(ActorVector& out, ActorContext& context, ActorId id) = 0;
    void claimActorContext(ActorContext& context) override;
    ActorVector produce() override;

private:
    int _threads;
    std::atomic<ActorId> _nextActorId;
    ActorContext* _context = nullptr;
};

/** @private */
template <class ActorT>
class DefaultActorProducer : public ParallelizedActorProducer {
public:
    using ParallelizedActorProducer::ParallelizedActorProducer;
    DefaultActorProducer() : DefaultActorProducer{ActorT::defaultName()} {}

    void produceInto(ActorVector& out, ActorContext& context, ActorId id) override {
        out.emplace_back(std::make_unique<ActorT>(context, id));
    }
};

}  // namespace genny

#endif  // HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
