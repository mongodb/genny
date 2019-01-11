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

#ifndef HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED
#define HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED

#include <memory>
#include <vector>

namespace genny {

struct Actor;

/**
 * A convenience typedef for the return value from ActorProducer.
 */
using ActorVector = typename std::vector<std::unique_ptr<Actor>>;


}  // namespace genny

#endif  // HEADER_728E42F5_3C88_4288_9D4A_945FA85DD895_INCLUDED
