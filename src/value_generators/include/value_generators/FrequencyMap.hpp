// Copyright 2022-present MongoDB Inc.
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

#ifndef HEADER_0BC4D6BC_FC92_4F1C_BAEA_26A633807830_INCLUDE
#define HEADER_0BC4D6BC_FC92_4F1C_BAEA_26A633807830_INCLUDE

#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace genny::v1 {

/**
 * Genny frequency map. A list of pairs <string, count>
 *
 * The list is unsorted.
 */
class FrequencyMap {
public:
    /**
     * Add an item and a count to the back of the list of items in the map.
     */
    void push_back(std::string name, size_t count) {
        _list.push_back({name, count});
    }

    /**
     * Take a instance of one of the items, decrements the count.
     *
     * It the count for the item goes to zero, it removes the item.
     *
     * Throws an error if there are no items in the map.
     */
    std::string take(size_t index) {
        if (index >= _list.size()) {
            throw std::range_error("Out of bounds of frequency map");
        }

        auto& pair = _list[index];
        auto ret = pair.first;
        pair.second--;

        // We have taken all the entries for this element
        if (pair.second == 0) {
            _list.erase(_list.begin() + index);
        }

        return ret;
    }

    /**
     * Returns the number of elements with counts
     */
    size_t size() const {
        return _list.size();
    }

    /**
     * Returns a total size of the frequency map
     */
    size_t total_count() const {
        size_t total_count = 0;

        for (const auto& p : _list) {
            total_count += p.second;
        }

        return total_count;
    }

private:
    std::vector<std::pair<std::string, uint64_t>> _list;
};

}  // namespace genny::v1

#endif