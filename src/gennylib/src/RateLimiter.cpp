#include <gennylib/v1/RateLimiter.hpp>

#include <thread>

#include <boost/log/trivial.hpp>

namespace genny::v1 {
RateLimiter::RateLimiter() : RateLimiter(Options{}) {}

RateLimiter::RateLimiter(const Options& options)
    : _options{options}, _state{std::make_unique<State>()} {}

void RateLimiter::waitFor(DurationT sleepMS) {
    if (sleepMS.count() > 0) {
        std::this_thread::sleep_for(sleepMS);
    }
}

void RateLimiter::waitUntil(TimeT stopTime) {
    std::this_thread::sleep_until(stopTime);
}

void RateLimiter::waitUntilNext() {
    auto hasEndTime = _state->startTime != _state->endTime;
    if (_state->status != Status::kInactive && hasEndTime) {
        waitUntil(_state->endTime);
    }

    _scheduleNext();
}

void RateLimiter::start() {
    _scheduleNext();
}

void RateLimiter::stop() {
    _state->status = Status::kInactive;
}

void RateLimiter::_scheduleNext() {
    auto status = std::exchange(_state->status, Status::kRunning);
    _state->startTime = (status == Status::kRunning) ? _state->endTime : ClockT::now();
    _state->endTime = _state->startTime + _options.minPeriod;
    ++_state->generation;
}

}  // namespace genny::v1

