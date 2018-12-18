#ifndef HEADER_32412A69_F128_4BC8_8335_520EE35F5381
#define HEADER_32412A69_F128_4BC8_8335_520EE35F5381

#include <gennylib/Actor.hpp>
#include <gennylib/context.hpp>
#include <gennylib/PhaseLoop.hpp>

namespace genny::actor {

/**
 * RunCommand is an actor that performs database and admin commands on a database. The
 * actor records the latency of each command run.
 *
 *
 * Example:
 *
 * ```yaml
 * Actors:
 * - Name: MultipleOperations
 *   Type: RunCommand
 *   Database: test
 *   Operations:
 *   - MetricsName: ServerStatus
 *     OperationName: RunCommand
 *     OperationCommand:
 *       serverStatus: 1
 *   - OperationName: RunCommand
 *     OperationCommand:
 *       find: scores
 *       filter: { rating: { $gte: 50 } }
 * - Name: SingleAdminOperation
 *   Type: AdminCommand
 *   Database: admin
 *   Phases:
 *   - Repeat: 5
 *     MetricsName: CurrentOp
 *     Operation:
 *       OperationName: RunCommand
 *       OperationCommand:
 *         currentOp: 1
 * ```
 */

class RunCommand : public Actor {

public:
    explicit RunCommand(ActorContext& context, const unsigned int threads);
    ~RunCommand() = default;

    static std::string_view defaultName() {
        return "RunCommand";
    }

    void run() override;

private:
    struct PhaseConfig;
    std::mt19937_64 _rng;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};


}  // namespace genny::actor

#endif  // HEADER_32412A69_F128_4BC8_8335_520EE35F5381
