#ifndef HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
#define HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED

#include <functional>

#include <gennylib/ActorVector.hpp>

namespace genny {

using ActorProducer = typename std::function<ActorVector(class ActorContext&)>;

}  // namespace genny

#endif  // HEADER_F7182B1D_27AF_4F90_9BB0_1ADF86FD1AEC_INCLUDED
