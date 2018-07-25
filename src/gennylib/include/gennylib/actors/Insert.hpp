#ifndef HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
#define HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED

#include <iostream>

#include <bsoncxx/json.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>

#include <gennylib/PhasedActor.hpp>

namespace genny::actor {

class Insert : public genny::PhasedActor {

public:
    explicit Insert(ActorContext& context,
                    const unsigned int thread,
                    std::string mongoUri = "mongodb://localhost:27017",
                    std::string dbName = "test");

    ~Insert() = default;

    static ActorVector producer(ActorContext& context);

private:
    void doPhase(int phase);

    metrics::Timer _outputTimer;
    metrics::Counter _operations;
    mongocxx::database _db;
    std::string _mongoUri;
    std::string _dbName;
    std::vector<std::string> _documents;
};

}  // namespace genny::actor

#endif  // HEADER_C7F4E568_590C_4D4D_B46F_766447E6AE31_INCLUDED
