#include "test.h"

#include <gennylib/context.hpp>
#include <gennylib/metrics.hpp>
#include <log.hh>

#include "ContextHelper.hpp"

using namespace genny;

std::atomic_int calls = 0;

class MyActor : public Actor {
public:
    MyActor(ActorContext &context) : Actor(context) {}
    void run() override {
        ++calls;
    }
};

class MyProducer : public ActorProducer {
public:
    MyProducer(const std::string_view &name) : ActorProducer(name) {}

    ActorVector produce(ActorContext& context) override {
        ActorVector out;
        out.emplace_back(std::make_unique<MyActor>(context));
        return out;
    }
};

TEST_CASE("Can register a new ActorProducer") {
    ContextHelper<MyProducer> helper {"MyActor", R"(
SchemaVersion: 2018-07-01
Actors:
- Type: MyActor
)"};

}