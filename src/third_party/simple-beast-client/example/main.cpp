/*
* simple-beast-client example
*/

#include <simple-beast-client/httpclient.hpp>
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[])
{
    std::cout << "Start up\n";

    try {
        boost::asio::io_context ioContext;

        // URL parsing validation tests.
        simple_http::url test{"http://test.com/target"};
        simple_http::url test2{"www.test.com/target2"};
#ifdef ENABLE_HTTPS
        simple_http::url test3{"https://test.com"};
#endif
        simple_http::url test4{"test.com:80"};
        simple_http::url test5{"http://33.com:400/target"};
#ifdef ENABLE_DIGEST
        simple_http::url test6{"http://user:pass@33.com"};
        simple_http::url test7{"http://user:pass@33.com:400/target?val=1&val2=2"};
#endif
        simple_http::url test8{"user@example.com"};

        assert(test.scheme() == "http");

        assert(test2.host() == "www.test.com");
        assert(test2.path() == "/target2");
        
#ifdef ENABLE_HTTPS
        assert(test3.scheme() == "https");
#endif
        
        assert(test4.host() == "test.com");
        assert(test4.port() == "80");

        assert(test5.host() == "33.com");
        assert(test5.port() == "400");

#ifdef ENABLE_DIGEST
        assert(test6.scheme() == "http");
        assert(test6.username() == "user");
        assert(test6.password() == "pass");

        assert(test7.port() == "400");
        assert(test7.path() == "/target");
        assert(test7.query() == "val=1&val2=2");
#endif

        assert(test8.username() == "user");
        assert(test8.host() == "example.com");

        simple_http::url testUserNameAndPassword{"https://user76:myP@55w0rd@example.com/path"};

        assert(testUserNameAndPassword.scheme() == "https");
        assert(testUserNameAndPassword.username() == "user76");
        assert(testUserNameAndPassword.password() == "myP@55w0rd");
        assert(testUserNameAndPassword.host() == "example.com");
        assert(testUserNameAndPassword.path() == "/path");

        // Test an invalid url string
        simple_http::url broken{"?this"};

        assert(!broken.valid());
        assert(broken.scheme().empty());
        assert(broken.host().empty());
        assert(broken.query().empty());

        simple_http::url copy = testUserNameAndPassword;

        assert(copy.scheme() == testUserNameAndPassword.scheme());
        assert(copy.username() == testUserNameAndPassword.username());
        assert(copy.password() == testUserNameAndPassword.password());
        assert(copy.host() == testUserNameAndPassword.host());
        assert(copy.target() == testUserNameAndPassword.target());

        simple_http::url by_parts{"example.com", "/path?query", "https", "8443", "user", "pass"};

        assert(by_parts.scheme() == "https");
        assert(by_parts.username() == "user");
        assert(by_parts.password() == "pass");
        assert(by_parts.host() == "example.com");
        assert(by_parts.port() == "8443");
        assert(by_parts.path() == "/path");
        assert(by_parts.query() == "query");

        copy = by_parts;

        assert(copy.scheme() == "https");
        assert(copy.username() == "user");
        assert(copy.password() == "pass");
        assert(copy.host() == "example.com");
        assert(copy.port() == "8443");
        assert(copy.path() == "/path");
        assert(copy.query() == "query");

        std::cout << "simple_http::url tests complete\n";

        // Run an asynchronous client test - connect with Digest Authentication
        {
            auto client = std::make_shared<simple_http::get_client>(
                ioContext,
                [](simple_http::empty_body_request& /*req*/,
                   simple_http::string_body_response& resp) {
                    // Display the response to the console.
                    std::cout << resp << '\n';
                });

            // Run the GET request to httpbin.org
            client->get(simple_http::url{
                "https://user:passwd@httpbin.org/digest-auth/auth/user/passwd/MD5/never"});
        }

        // Run another asynchronous client test - redirection and HTTPS connect.
        // This example shows the boost::beast request and response template classes.
        {
            auto client = std::make_shared<simple_http::get_client>(
                ioContext,
                [](boost::beast::http::request<boost::beast::http::empty_body>& /*req*/,
                   boost::beast::http::response<boost::beast::http::string_body>& resp) {
                    std::cout << resp << '\n';
                });
            client->setFailHandler([](const simple_http::empty_body_request& /*req*/,
                                      const simple_http::string_body_response& /*resp*/,
                                      simple_http::fail_reason /*fr*/, boost::string_view message) {
                std::cout << message << '\n';
            });
            // Connect to a url that redirects through a 301 response.
            // The 1 argument is the number of redirects to follow.
            client->get(
                simple_http::url{
                    "http://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org&status_code=301"},
                1);
        }

        // Run a POST client test
        {
            auto client = std::make_shared<simple_http::post_client>(
                ioContext,
                [](simple_http::string_body_request& /*req*/,
                   simple_http::string_body_response& resp) { std::cout << resp << '\n'; });

            client->post(simple_http::url{"http://httpbin.org/post"}, "username=RAvenGEr",
                         "application/x-www-form-urlencoded");
        }

        // Run the until requests are complete.
        ioContext.run();
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
