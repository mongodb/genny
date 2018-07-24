#ifndef HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
#define HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED

#include <functional>

#include <gennylib/ActorVector.hpp>

namespace genny {

/**
 * An ActorProducer maps from ActorContext -> vector of Actors.
 *
 * For the following YAML,
 *
 * <pre>
 *      SchemaVersion: 2018-07-01
 *      Actors:
 *      - Name: Foo
 *      - Name: Bar
 * </pre>
 *
 * each ActorProducer will be called twice: once with the ActorContext for
 * {Name:Foo} and another with the ActorContext for {Name:Bar}.
 *
 * Many ActorProducers will want to return an empty ActorVector if the
 * "Name" field is different from what they expect, but this is just
 * a convention.
 *
 * Actors may retain a reference to the ActorContext and/or parent
 * WorkloadContext, but it is recommended for performance that they
 * call context.get(...) only during their constructors and retain
 * refs or copies of config objects
 */
using ActorProducer = typename std::function<ActorVector(class ActorContext&)>;

}  // namespace genny

#endif  // HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
