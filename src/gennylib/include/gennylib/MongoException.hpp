#ifndef HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED
#define HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED

#include <boost/exception/all.hpp>
#include <boost/exception/error_info.hpp>

#include <bsoncxx/json.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include <iostream>

namespace genny {


/**
 * Wrapper around exceptions thrown by the MongoDB CXX driver to provide more context.
 *
 * More information can be added to MongoException by creating a `typedef boost::error_info`
 * for your info and pasing it down from runCommandHelper to MongoException.
 */
class MongoException : public boost::exception, public std::exception {

public:
    using BsonView = bsoncxx::document::view;

    // Dummy MongoException for testing.
    MongoException(const std::string& message = "") {
        *this << MongoException::Message("SOME MESSAGE");
    }

    MongoException(mongocxx::operation_exception& x,
                   MongoException::BsonView info,
                   const std::string& message = "") {

        *this << MongoException::Info(bsoncxx::to_json(info));

        if (x.raw_server_error() && !x.raw_server_error()->view().empty()) {
            *this << MongoException::ServerError(
                bsoncxx::to_json(x.raw_server_error().value().view()));
        }

        *this << MongoException::Message(message);
    }

private:
    /**
     * boost::error_info are tagged error messages. The first argument is a struct whose name is the
     * tag and the second argument is the type of the error message, the type must be supported by
     * boost::diagnostic_information().
     *
     * You can add a new error_info by streaming it to MongoException (i.e. *this <<
     * MyErrorInfo("uh-oh")).
     */
    using ServerError = boost::error_info<struct ServerResponse, std::string>;
    using Info = boost::error_info<struct InfoObject, std::string>;
    using Message = boost::error_info<struct Message, std::string>;
};
}  // namespace genny

#endif  // HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED
