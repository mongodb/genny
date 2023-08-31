// Copyright 2023-present MongoDB Inc.
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

#ifndef HEADER_KAFKATESTFIXTURE_INCLUDED
#define HEADER_KAFKATESTFIXTURE_INCLUDED

#include <string>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

namespace genny::testing {

class KafkaTestFixture {
public:
    static const std::string kDefaultBootstrapServers;

    KafkaTestFixture () {}

    static std::string connectionUri();
}; // class KafkaTestFixture

}  // namespace genny::testing

#endif  // HEADER_KAFKATESTFIXTURE_INCLUDED
