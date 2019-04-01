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

#ifndef HEADER_6FFA3C56_7A9D_48B9_950F_6373244E3CC1_INCLUDED
#define HEADER_6FFA3C56_7A9D_48B9_950F_6373244E3CC1_INCLUDED

#include <memory>
#include <string>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>

// TODO: move to v1
namespace genny {


class Appendable {
public:
    virtual ~Appendable() = default;
    virtual void append(const std::string& key, bsoncxx::builder::basic::document& builder) = 0;
    virtual void append(bsoncxx::builder::basic::array& builder) = 0;
};

using UniqueAppendable = std::unique_ptr<Appendable>;


template <typename T>
class ConstantAppender : public Appendable {
public:
    explicit ConstantAppender(T value) : _value{value} {}
    explicit ConstantAppender() : _value{} {}
    T evaluate() {
        return _value;
    }
    ~ConstantAppender() override = default;
    void append(const std::string& key, bsoncxx::builder::basic::document& builder) override {
        builder.append(bsoncxx::builder::basic::kvp(key, _value));
    }
    void append(bsoncxx::builder::basic::array& builder) override {
        builder.append(_value);
    }

protected:
    T _value;
};


}  // namespace genny

#endif  // HEADER_6FFA3C56_7A9D_48B9_950F_6373244E3CC1_INCLUDED
