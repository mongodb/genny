// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
#define HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

namespace genny::testing {

class MongoTestFixture {
public:
    MongoTestFixture() : instance{mongocxx::instance::current()}, client{connectionUri()} {}

    void dropAllDatabases();

    static mongocxx::uri connectionUri();

private:
    class ApmEvent {
        public:
        ApmEvent(const std::string& command_name_, const bsoncxx::document::value& document_)
            : command_name(command_name_), value(document_), command(value.view()) {}

        std::string command_name;
        bsoncxx::document::value value;
        bsoncxx::document::view command;
    };
    mongocxx::instance& instance;

protected:
    mongocxx::client client;
    std::vector<ApmEvent> events;
    std::function<void(const mongocxx::events::command_started_event&)> apmCallback =
        [&](const mongocxx::events::command_started_event& event) {
            std::string command_name{event.command_name().data()};

            // Ignore auth commands like "saslStart", and handshakes with "isMaster".
            std::string sasl{"sasl"};
            if (event.command_name().substr(0, sasl.size()).compare(sasl) == 0 ||
                command_name.compare("isMaster") == 0) {
                return;
            }

            events.emplace_back(command_name, bsoncxx::document::value(event.command()));
        };
    void clearEvents(){
        events.clear();
    };

};
}  // namespace genny::testing

#endif  // HEADER_D9091084_CB09_4108_A553_5D0EC18C132F_INCLUDED
