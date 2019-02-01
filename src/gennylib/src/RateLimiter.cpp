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

#include <gennylib/v1/RateLimiter.hpp>

#include <thread>

#include <boost/log/trivial.hpp>

namespace genny::v1 {

RateLimiter::~RateLimiter(){};

RateLimiterSimple::RateLimiterSimple() : RateLimiterSimple(Options{}) {}

RateLimiterSimple::RateLimiterSimple(const Options& options)
    : _options{options}, _state{std::make_unique<State>()} {}

RateLimiterSimple::~RateLimiterSimple() {}

void RateLimiterSimple::waitFor(Duration sleepDuration) {
    if (sleepDuration != Duration::zero()) {
        std::this_thread::sleep_for(sleepDuration);
    }
}

void RateLimiterSimple::waitUntil(RateLimiter::TimeT stopTime) {
    std::this_thread::sleep_until(stopTime);
}

void RateLimiterSimple::waitUntilNext() {
    auto hasEndTime = _state->startTime != _state->endTime;
    if (_state->status != Status::kInactive && hasEndTime) {
        waitUntil(_state->endTime);
    }

    _scheduleNext();
}

void RateLimiterSimple::start() {
    _scheduleNext();
}

void RateLimiterSimple::stop() {
    _state->status = Status::kInactive;
}

void RateLimiterSimple::_scheduleNext() {
    auto status = std::exchange(_state->status, Status::kRunning);
    _state->startTime = (status == Status::kRunning) ? _state->endTime : ClockT::now();
    _state->endTime = _state->startTime + _options.minPeriod;
    ++_state->generation;
}

}  // namespace genny::v1
