#include <gennylib/Uri.hpp>

#include <mongoc/mongoc.h>
#include <mongocxx/uri.hpp>

#include <iostream>

namespace genny {
namespace {
    Uri::DataT _wrapData(std::string_view rawUri){
        auto* data = ::mongoc_uri_new(rawUri.data());
        if(data == nullptr)
            return {};

        auto delFun = [](mongoc_uri_t* ptr) -> void { ::mongoc_uri_destroy(ptr); };

        Uri::DataT wrappedPtr(data, std::move(delFun));
        return wrappedPtr;
    }
}

Uri::Uri(std::string_view rawUri) : _data(_wrapData(rawUri)) {}
mongocxx::uri Uri::toMongoCxxUri() const {
    auto rawUri = ::mongoc_uri_get_string(_data.get());
    return mongocxx::uri{rawUri};
}
bool Uri::isValid() const{
    return static_cast<bool>(_data);
}

} // namespace genny
