#include <gennylib/ActorProducer.hpp>

#include <gennylib/context.hpp>

namespace genny {

ActorVector ParallelizedActorProducer::produce(ActorContext& context) {
    ActorVector out;

    auto threads = context.get<int, /* Required = */ false>("Threads").value_or(1);
    for (decltype(threads) i = 0; i < threads; ++i) {
        produceInto(out, context);
    }
    return out;
}

}  // namespace genny
