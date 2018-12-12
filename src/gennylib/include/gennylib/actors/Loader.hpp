#ifndef HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
#define HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

namespace genny::actor {

/**
 * Prepares a database for testing. For use with {@code MultiCollectionUpdate} and {@code
 * MultiCollectionQuery} actors. It loads a set of documents into multiple collections with
 * indexes. Each collection is identically configured. The document shape, number of documents,
 * number of collections, and list of indexes are all adjustable from the yaml configuration.
 */
class Loader : public Actor {

public:
    explicit Loader(ActorContext& context, uint thread);
    ~Loader() = default;

    void run() override;

    static ActorVector producer(ActorContext& context);

private:
    struct PhaseConfig;
    std::mt19937_64 _rng;
    metrics::Timer _totalBulkLoadTimer;
    metrics::Timer _individualBulkLoadTimer;
    metrics::Timer _indexBuildTimer;
    mongocxx::pool::entry _client;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_1E8F3397_B82B_4814_9BB1_6C6D2E046E3A
