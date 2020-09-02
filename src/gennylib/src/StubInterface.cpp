#include <metrics/v2/event.hpp>

namespace genny::metrics::internals::v2 {

/*
 * Static initialization of the global channel pool.
 *
 * Located in some random cpp file because it can't be initialized in the declaration header.
 */
std::vector<std::shared_ptr<grpc::Channel>> CollectorStubInterface::_channels = std::vector<std::shared_ptr<grpc::Channel>>();

/*
 * Static initialization of the most recently-used channel.
 */
std::atomic<size_t> CollectorStubInterface::_curChannel = 0;

}  // namespace genny::metrics::internals::v2
