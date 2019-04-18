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

#include <testlib/helpers.hpp>

namespace {

TEST_CASE("MultiLineRegexMatch") {
    std::string actual = R"(
/Users/rtimmons/Projects/genny/src/cast_core/src/CrudActor.cpp(244): Throw in function void (anonymous namespace)::BaseOperation::doBlock(metrics::Operation &, F &&) [F = (lambda at /Users/rtimmons/Projects/genny/src/cast_core/src/CrudActor.cpp:346:35)]
Dynamic exception type: boost::exception_detail::clone_impl<genny::MongoException>
std::exception::what: std::exception
[genny::InfoObject*] = { }
[genny::ServerResponse*] = { "nInserted" : 0, "nMatched" : 0, "nModified" : 0, "nRemoved" : 0, "nUpserted" : 0, "writeErrors" : [ { "index" : 0, "code" : 11000, "errmsg" : "E11000 duplicate key error collection: mydb.test index: a_1 dup key: { : 100 }" } ] }
[genny::Message*] =
)";
    SECTION("Simple match") {
        std::string rex = ".*duplicate key error.*";
        REQUIRE_THAT(actual, genny::MultiLineRegexMatch(rex));
    }
}

}  // namespace
