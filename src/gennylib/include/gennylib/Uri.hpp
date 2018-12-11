#pragma once

#include <mongoc/mongoc.h>

#include <functional>
#include <memory>
#include <set>
#include <string_view>

#include <mongocxx/uri.hpp>

namespace genny {

class Uri {
public:
    using DataT = std::unique_ptr<mongoc_uri_t, std::function<void(mongoc_uri_t*)>>;

public:
    Uri(std::string_view rawUri);

    mongocxx::v_noabi::uri toMongoCxxUri() const;
    bool isValid() const;

private:
    DataT _data;
};

} // namespace genny
