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

#include <cast_core/actors/LoggingActor.hpp>

#include <memory>
#include <optional>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/regex.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/InvalidConfigurationException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace {

constexpr auto MESSAGE = "Invalid RepeatEvery %s: (%s): Must be either \"N iterations\" or a TimeSpec (e.g. \"1 second\").";

std::optional<genny::IntegerSpec> createIters(const std::string& repeatEvery) {
    boost::regex expr{R"(^\s*(\d+)+\s+[iI]terations?$)"};
    boost::smatch what;
    if (boost::regex_search(repeatEvery, what, expr)) {
        const auto spec = what[1];
        try {
            return std::make_optional(genny::IntegerSpec(boost::lexical_cast<int64_t>(spec)));
        } catch(const boost::bad_lexical_cast &) {
            std::stringstream str;
            str << boost::format(MESSAGE) % repeatEvery % "bad_lexical_cast";
            BOOST_THROW_EXCEPTION(genny::InvalidConfigurationException(str.str()));
        }
    } else {
        std::stringstream str;
        str << boost::format(MESSAGE) % repeatEvery % "regex mismatch";
        BOOST_THROW_EXCEPTION(genny::InvalidConfigurationException(str.str()));
    }
}

std::optional<genny::TimeSpec> createTime(const std::string& repeatEvery) {

}

}  // namespace

namespace genny::actor {

struct LoggingActor::PhaseConfig {
    std::optional<TimeSpec> time;
    std::optional<IntegerSpec> iters;

    explicit PhaseConfig(PhaseContext& phaseContex) {}
    void report() {}
};

void LoggingActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            config->report();
        }
    }
}

LoggingActor::LoggingActor(genny::ActorContext& context)
    : Actor{context}, _loop{context} {}

namespace {
auto registerLoggingActor = Cast::registerDefault<LoggingActor>();
}  // namespace
}  // namespace genny::actor
