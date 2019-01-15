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
