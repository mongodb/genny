#ifndef HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
#define HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

namespace genny::testing {

class MongoTestFixture {
public:
    MongoTestFixture() : instance(mongocxx::instance::current()), client(connectionUri()) {}

protected:
    void dropAllDatabases();

    mongocxx::instance& instance;
    mongocxx::client client;

    static mongocxx::uri connectionUri();
};
}  // namespace genny::testing

#endif  // HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
