#include <boost/exception/all.hpp>
#include <mongocxx/exception/operation_exception.hpp>

namespace genny {

using BsonView = bsoncxx::document::view;

/**
 * Wrapper around exceptions thrown by the MongoDB CXX driver to provide more context.
 *
 * More information can be added to MongoException by creating a `typedef boost::error_info`
 * for your info and pasing it down from runCommandHelper to MongoException.
 */
class MongoException : public boost::exception, public std::exception {

public:
    MongoException(mongocxx::operation_exception& x, BsonView command, const std::string& otherInfo = "") {

        *this << MongoException::Info(bsoncxx::to_json(command));

        if (x.raw_server_error() && !x.raw_server_error()->view().empty()) {
            *this << MongoException::ServerError(bsoncxx::to_json(x.raw_server_error().value().view()));
        }

        *this << MongoException::Message(otherInfo);
    }
private:
    using ServerError = boost::error_info<struct server_response, std::string>;
    using Info = boost::error_info<struct command_object, std::string>;
    using Message = boost::error_info<struct other_info, std::string>;
};
}