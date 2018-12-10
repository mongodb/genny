#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>

#include <bsoncxx/json.hpp>

#include <gennylib/operations/RunCommand.hpp>
#include <gennylib/value_generators.hpp>
#include <log.hh>


namespace {}

genny::operation::RunCommand::RunCommand(const genny::OperationContext& operationContext,
                                         mongocxx::client& client,
                                         std::mt19937_64& rng)
    : Operation{operationContext, client},
      _documentTemplate{genny::value_generators::makeDoc(operationContext.get("Command"), rng)} {}

void genny::operation::RunCommand::run() {
    bsoncxx::builder::stream::document document{};
    auto view = _documentTemplate->view(document);
    BOOST_LOG_TRIVIAL(info) << " Running command: " << bsoncxx::to_json(view);
    _database.run_command(view);
}
