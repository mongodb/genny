#ifndef HEADER_18839D36_A5FC_4476_B461_9E66D73B3192_INCLUDED
#define HEADER_18839D36_A5FC_4476_B461_9E66D73B3192_INCLUDED

#include <gennylib/context.hpp>

#include <mongocxx/client.hpp>

namespace genny {

class Operation {
public:
    Operation(const OperationContext& operationContext, mongocxx::client& client)
        : _collection(client[operationContext.get<std::string>("Database")]
                            [operationContext.get<std::string>("Collection")]),
          _database(client[operationContext.get<std::string>("Database")]) {}

    virtual ~Operation() = default;
    virtual void run() = 0;

protected:
    mongocxx::collection _collection;
    mongocxx::database _database;
};

}  // namespace genny

#endif  // HEADER_18839D36_A5FC_4476_B461_9E66D73B3192_INCLUDED
