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

#ifndef HEADER_05BC1BA0_23B0_41BE_AEC3_DAA04EF985AF_INCLUDED
#define HEADER_05BC1BA0_23B0_41BE_AEC3_DAA04EF985AF_INCLUDED


#include <crud_operations/OptionsConversion.hpp>

#include <chrono>
#include <memory>
#include <utility>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <bsoncxx/json.hpp>
#include <bsoncxx/string/to_string.hpp>

#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>
#include <gennylib/conventions.hpp>

using BsonView = bsoncxx::document::view;
using bsoncxx::type;

namespace genny::crud_operations {

using namespace genny;

enum class ThrowMode {
    kSwallow,
    kRethrow,
    kSwallowAndRecord,  // Record failed operations but don't throw.
};
ThrowMode decodeThrowMode(const Node& operation, PhaseContext& phaseContext);

// A large number of subclasses have
// - metrics::Operation
// - mongocxx::collection
// - bool (_onSession)
// it may make sense to add a CollectionAwareBaseOperation
// or something intermediate class to eliminate some
// duplication.
class BaseOperation {
    ThrowMode throwMode;

    using MaybeDoc = std::optional<bsoncxx::document::value>;

    explicit BaseOperation(PhaseContext& phaseContext, const Node& operation)
        : throwMode{decodeThrowMode(operation, phaseContext)} {}

    template <typename F>
    void doBlock(metrics::Operation& op, F&& f);

    virtual void run(mongocxx::client_session& session) = 0;
    virtual ~BaseOperation() = default;
};

using OpCallback = std::function<std::unique_ptr<BaseOperation>(const Node&,
                                                                bool,
                                                                mongocxx::collection,
                                                                metrics::Operation,
                                                                PhaseContext& context,
                                                                ActorId id)>;

std::unordered_map<std::string, OpCallback&> getOpConstructors();

}  // namespace genny::crud_operations

#endif  // HEADER_05BC1BA0_23B0_41BE_AEC3_DAA04EF985AF_INCLUDED
