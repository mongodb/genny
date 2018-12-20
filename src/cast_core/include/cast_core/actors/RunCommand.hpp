#ifndef HEADER_32412A69_F128_4BC8_8335_520EE35F5381
#define HEADER_32412A69_F128_4BC8_8335_520EE35F5381

#include <gennylib/Actor.hpp>
#include <gennylib/context.hpp>
#include <gennylib/PhaseLoop.hpp>

namespace genny::actor {

/**
 * RunCommand is an actor that performs database and admin commands on a database. The
 * actor records the latency of each command run. If no database value is provided for
 * an actor, then the operations will run on the 'admin' database by default.
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
    struct Operation;
    struct PhaseState;

public:
    explicit RunCommand(ActorContext& context);
    ~RunCommand() = default;

    static std::string_view defaultName() {
        return "RunCommand";
    }

    void run() override;

private:
    std::mt19937_64 _rng;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseState> _loop;
};


}  // namespace genny::actor

#endif  // HEADER_32412A69_F128_4BC8_8335_520EE35F5381
