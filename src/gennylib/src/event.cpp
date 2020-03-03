#include <metrics/v2/event.hpp>

namespace genny::metrics::internals::v2 {

std::unique_ptr<poplar::PoplarEventCollector::StubInterface> CollectorStubInterface::_stub(nullptr);

}
