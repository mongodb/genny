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

#include <metrics/metrics.hpp>

namespace genny::metrics {
namespace v1 {

template <typename ClockSource>
OperationContextT<ClockSource>::OperationContextT(v1::OperationImpl<ClockSource>& op)
    : _op{std::addressof(op)}, _started{ClockSource::now()} {}

template <typename ClockSource>
OperationContextT<ClockSource>::OperationContextT(OperationContextT<ClockSource>&& rhs) noexcept
    : _op{std::move(rhs._op)},
      _started{std::move(rhs._started)},
      _isClosed{std::move(rhs._isClosed)},
      _totalBytes{std::move(rhs._totalBytes)},
      _totalOps{std::move(rhs._totalOps)} {
    rhs._isClosed = true;
}

template <typename ClockSource>
OperationContextT<ClockSource>::~OperationContextT() {
    if (!_isClosed) {
        BOOST_LOG_TRIVIAL(warning)
            << "Metrics not reported because operation '" << this->_op->getOpName()
            << "' did not close with success() or fail().";
    }
}

template <typename ClockSource>
void OperationContextT<ClockSource>::addBytes(const count_type& size) {
    _totalBytes += size;
}

template <typename ClockSource>
void OperationContextT<ClockSource>::addOps(const count_type& size) {
    _totalOps += size;
}

template <typename ClockSource>
void OperationContextT<ClockSource>::success() {
    this->report();
    _isClosed = true;
}

/**
 * An operation does not report metrics upon failure.
 */
template <typename ClockSource>
void OperationContextT<ClockSource>::fail() {
    _isClosed = true;
}

template <typename ClockSource>
void OperationContextT<ClockSource>::report() {
    this->_op->reportSuccess(_started);
    this->_op->reportNumBytes(_totalBytes);
    this->_op->reportNumOps(_totalOps);
}

template class OperationContextT<MetricsClockSource>;

}  // namespace v1
}  // namespace genny::metrics
