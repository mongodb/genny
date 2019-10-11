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

#ifndef HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED
#define HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED

/**
 * @namespace genny::metrics::v1 this namespace is private and only intended to be used by genny's
 * own internals. No types from the genny::metrics::v1 namespace should ever be typed directly into
 * the implementation of an actor.
 */
namespace genny::metrics::v1 {

/**
 * The ReporterT class is given read-only access to the metrics data for the purposes of recording
 * it to a file.
 */
template <typename MetricsClockSource>
class ReporterT;

/**
 * The passkey idiom is way for a class to govern how its private members can be accessed by another
 * class. It can be thought of as a finer-grained way to express friendship in C++. The passkey
 * idiom works by defining a class with a private constructor and having that class be friends with
 * the class seeking access. Since the constructor is private, only the class seeking access is able
 * to construct instances of it (though could possibly share them). This means that the governing
 * class can then define a public function that requires it as an argument and still be ensured that
 * only the class seeking access is able to call that function.
 *
 * @see https://arne-mertz.de/2016/10/passkey-idiom/
 */
class Permission {
private:
    constexpr Permission() = default;

    template <typename MetricsClockSource>
    friend class ReporterT;
};

static_assert(std::is_empty<Permission>::value, "empty");

}  // namespace genny::metrics::v1

#endif  // HEADER_60839F59_EE36_4926_A72D_3D4B68DEA4F9_INCLUDED
