#ifndef HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
#define HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED

#include <mongocxx/client.hpp>

namespace genny::testing {

class MongoTestFixture {
public:
    MongoTestFixture() : client(kConnectionString) {}

protected:
    void dropAllDatabases();

    mongocxx::client client;

private:
    static const mongocxx::uri kConnectionString;
};
}  // namespace genny::testing

#endif  // HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
