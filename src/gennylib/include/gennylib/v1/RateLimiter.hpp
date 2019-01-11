#ifndef HEADER_C68990DE_CA18_4104_9D1F_070CDC3077B0
#define HEADER_C68990DE_CA18_4104_9D1F_070CDC3077B0

#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <type_traits>

#include <gennylib/config/RateLimiterOptions.hpp>

namespace genny::v1 {

/**
 * RateLimiter is a utility class that enforces time scheduling on a function.
 *
 * As a general set of features, for every call to `run()`, a RateLimiter may:
 * 1. Make sure the function passed to `run()` is invoked at least a minimum duration after the last
 *    invocation of `run()`.
 * 2. Enforce a sleep of a specified duration before the function passed to `run()` is invoked.
 * 3. Enforce a sleep of a specified duration after the function passed to `run()` is invoked.
 *
 * As an intentional choice, RateLimiter has a strong API that obscures the actual implementation.
 * As of now, it uses simple sleeps. The preferred mechanism would be to have a scheduler that
 * notifies each RateLimiter at specific deadlines.
 */
class RateLimiter {
public:
    using Generation = int64_t;
    using Options = config::RateLimiterOptions;

    using ClockT = std::chrono::steady_clock;
    using TimeT = typename ClockT::time_point;
    using DurationT = std::chrono::milliseconds;

    /**
     * Status is a very simple state enumeration.
     * * kInactive means that the next `run()` will be allowed to execute immediately.
     * * kRunning means that the next `run()` will execute after the underlying period has passed.
     */
    enum class Status {
        kInactive = 0,
        kRunning,
    };

    /**
     * A state object showing whether rate limiting is active and the details around that limiting.
     */
    struct State {
        Status status = Status::kInactive;

        TimeT startTime = TimeT::min();
        TimeT endTime = TimeT::min();

        Generation generation = -1;
    };

public:
    RateLimiter(const Options& options);

    /**
     * Block execution for a specified duration
     * @param sleepMS   A duration after which to unblock
     */
    void waitFor(DurationT sleepMS);

    /**
     * Block execution until a designated time
     * @param   stopTime    A time point until which to unblock
     */
    void waitUntil(TimeT stopTime);

    /**
     * Block execution until the current period is over and start the next
     *
     * If we have run before, wait until our minimum period is next.
     * If we have not run before, return immediately.
     * In either case, set the endTime for the coming period.
     */
    void waitUntilNext();

    /**
     * Run the given function with certain timing guarantees
     * @param fun An Callable object to invoke
     */
    template<typename F>
    void run(F&& fun){
        // Wait until we have surpassed our minimum period 
        waitUntilNext();

        // Wait for a specified amount of time before
        waitFor(_options.preSleep);

        // Run the actual function
        std::invoke(std::forward<F>(fun));

        // Wait for a specified amount of time after
        waitFor(_options.postSleep);
    }

    /**
     * Set the endTime for the coming period and mark ourselves as running
     */
    void start();

    /**
     * Mark ourselves as no longer running
     */
    void stop();

    /**
     * Return a constant refence to the current rate limit state
     */
    const State & state() const {
        return *_state;
    }

private:
    void _scheduleNext();

    Options _options;
    std::unique_ptr<State> _state;
};

}  // namespace genny::v1
#endif  // HEADER_C68990DE_CA18_4104_9D1F_070CDC3077B0
