#ifndef CLIENT_PRIVATE_HPP
#define CLIENT_PRIVATE_HPP

#include "url.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/utility/string_view.hpp>
#include <cppcodec/base64_default_rfc4648.hpp> // Used for basic authentication.
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#if defined(ENABLE_HTTPS) || defined(ENABLE_DIGEST)
#ifndef ENABLE_DIGEST
#define ENABLE_DIGEST
#endif
#include "digestauthenticator.hpp"
#endif

namespace simple_http {

enum client_state
{
    Resolve,
    Connect,
    Handshake,
    RequestSend,
    Header,
    Contents,
    Complete
};

enum fail_reason
{
    Unknown,
    FormatError,
    ResolveError,
    ConnectionError,
    HandshakeError,
    Timeout,
    WriteError,
    ReadError
};

using empty_body_request = boost::beast::http::request<boost::beast::http::empty_body>;
using string_body_request = boost::beast::http::request<boost::beast::http::string_body>;
using empty_body_response = boost::beast::http::response<boost::beast::http::empty_body>;
using string_body_response = boost::beast::http::response<boost::beast::http::string_body>;

template<typename RequestBody, typename ResponseBody>
class client_private_http;

template<typename RequestBody, typename ResponseBody>
class client_private_ssl;

template<typename RequestBody, typename ResponseBody>
class basic_client;

template<typename RequestBody, typename ResponseBody>
class client_private
{
    using ResponseParser = boost::beast::http::response_parser<ResponseBody>;
    using HttpPrivate = client_private_http<RequestBody, ResponseBody>;
    using SslPrivate = client_private_ssl<RequestBody, ResponseBody>;
    using BodyValue = typename RequestBody::value_type;

public:
    static std::shared_ptr<client_private<RequestBody, ResponseBody>> privateForRequest(
        const url& uri, std::shared_ptr<basic_client<RequestBody, ResponseBody>> cl)
    {
        if (uri.scheme() == url::SchemeHttp()) {
            return std::make_shared<HttpPrivate>(cl->m_io, cl);
        }
#ifdef ENABLE_HTTPS
        if (uri.scheme() == url::SchemeHttps()) {
            return std::make_shared<SslPrivate>(cl->m_io, cl);
        }
#endif
        cl->failure(FormatError, "Error unsupported scheme.");
        return nullptr;
    }

    void performRequest(const url& uri, boost::beast::http::verb method, BodyValue requestBody,
                        boost::string_view contentType, int maxRedirects, bool basicAuth,
                        int version)
    {
        c->m_maxRedirects = maxRedirects;
        m_url = uri;
        m_request.version(version);
        m_request.method(method);
        m_request.target(m_url.target());
        m_request.set(boost::beast::http::field::host, m_url.host());
        m_request.set(boost::beast::http::field::user_agent, c->userAgent());
        if (!contentType.empty()) {
            m_request.set(boost::beast::http::field::content_type, contentType);
        }
        if (method == boost::beast::http::verb::post) {
            m_request.body() = requestBody;
            m_request.prepare_payload();
        }
        if (basicAuth) {
            generateBasicAuthentication();
        }
        startTimeout();
        startResolve();
    }

    void abort()
    {
        boost::system::error_code ec;
        c->m_state = Complete;
        m_timeout.cancel();
        closeSocket(ec);
        if (c->m_responseHandler) {
            c->m_responseHandler = nullptr;
        }
        if (c->m_failHandler) {
            c->m_failHandler = nullptr;
        }
        // not_connected happens sometimes so don't bother reporting it.
        if (ec && ec != boost::system::errc::not_connected) {
            c->failure(Unknown, "Unexpected Error: " + ec.message());
        }
    }

protected:
    explicit client_private(boost::asio::io_context& io,
                            std::shared_ptr<basic_client<RequestBody, ResponseBody>> cl)
      : c{cl}, m_resolver{io}, m_timeout{io}
    {}

    virtual ~client_private() {}

    void startTimeout()
    {
        m_timeout.expires_from_now(boost::posix_time::milliseconds{c->m_timeoutMs});
        m_timeout.async_wait(
            std::bind(&client_private::checkTimeout, shared_from_this(), std::placeholders::_1));
    }

    void checkTimeout(const boost::system::error_code& ec)
    {
        if (!ec) {
            std::string stateString;
            switch (c->m_state) {
            case Resolve:
                stateString = "resolve";
                break;
            case Connect:
                stateString = "connect";
                break;
            case Handshake:
                stateString = "handshake";
                break;
            case RequestSend:
                stateString = "request";
                break;
            case Header:
                stateString = "header";
                break;
            case Contents:
                stateString = "contents";
                break;
            case Complete:
                return;
            }
            c->failure(Timeout, "Transfer timeout during " + stateString);
            abort();
        } else if (ec != boost::asio::error::operation_aborted) {
            fail(Timeout, "Transfer timer error: " + ec.message());
        }
    }

    void resetTimeout(client_state state)
    {
        c->m_state = state;
        auto self(shared_from_this());
        m_timeout.expires_from_now(boost::posix_time::milliseconds{c->m_timeoutMs});
    }

    void startResolve()
    {
        resetTimeout(Resolve);
        m_resolver.async_resolve(m_url.host().to_string(), m_url.port().to_string(),
                                 std::bind(&client_private::onResolve, shared_from_this(),
                                           std::placeholders::_1, std::placeholders::_2));
    }

    void onResolve(boost::system::error_code ec,
                   boost::asio::ip::tcp::resolver::results_type results)
    {
        if (ec) {
            fail(ResolveError, "Error resolving target: " + ec.message());
            return;
        }
        m_resolveResults = results;
        connectSocket();
    }

    void onConnect(boost::system::error_code ec)
    {
        if (ec) {
            fail(ConnectionError, "Error connecting: " + ec.message());
            return;
        }
        sendRequest();
    }

    void clearResponse()
    {
        m_responseParser.~ResponseParser();
        new (&m_responseParser) ResponseParser{};
    }

    void onWrite(boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            fail(WriteError, "Error writing request: " + ec.message());
            return;
        }
        initiateReadHeader();
    }

    void onReadHeader(boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            // Error during read of header.
            fail(ReadError, "Error in response header: " + ec.message());
            return;
        }
        initiateRead();
    }

    void onRead(boost::system::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            fail(ReadError, "Error reading response: " + ec.message());
            return;
        }
        m_timeout.cancel(); // Finished with connection before this point.
        if (handle()) {
            abort(); // This gracefully closes the client connection.
        }
    }

    bool handle()
    {
        const std::set<boost::beast::http::status> redirectCodes{
            boost::beast::http::status::moved_permanently, boost::beast::http::status::found,
            boost::beast::http::status::temporary_redirect};
        if (m_responseParser.get().result() == boost::beast::http::status::unauthorized) {
            if (generateAuthentication(
                    m_responseParser.get()[boost::beast::http::field::www_authenticate])) {
                // Request again with authentication!
                if (m_responseParser.keep_alive()) {
                    sendRequest();
                } else {
                    connectSocket();
                }
                return false;
            }
        } else if (c->m_maxRedirects > 0 && redirectCodes.count(m_responseParser.get().result())) {
            // follow redirect.
            url newLocation{m_responseParser.get()[boost::beast::http::field::location]};
            if (newLocation.host() == m_url.host() && newLocation.port() == m_url.port()) {
                m_url = std::move(newLocation);
                if (m_responseParser.keep_alive()) {
                    sendRequest();
                } else {
                    connectSocket();
                }
            } else {
                // Request is for a new server
                c->performRequest(std::move(newLocation), m_request.method(), m_request.body(),
                                  m_request[boost::beast::http::field::content_type],
                                  c->m_maxRedirects - 1, c->m_basicAuthForce, m_request.version());
            }
            return false;
        }
        try {
            c->m_responseHandler(m_request, m_responseParser.get());
        } catch (std::bad_function_call& /*f*/) {
            // Discard silently.
        }
        return true;
    }

    void fail(fail_reason reason, boost::string_view message)
    {
        try {
            c->m_failHandler(m_request, m_responseParser.get(), reason, message);
        } catch (std::bad_function_call& /*f*/) {
            // Discard silently.
        }
        abort();
    }

    bool generateAuthentication(boost::string_view authenticate)
    {
        if (c->m_username.empty() || c->m_password.empty() ||
            !m_request[boost::beast::http::field::authorization].empty()) {
            return false;
        }
        if (boost::ifind_first(authenticate, "digest")) {
#ifdef ENABLE_DIGEST
            return generateDigestAuth(authenticate, boost::string_view());
#else
            return false;
#endif
        } else {
            generateBasicAuthentication();
            return true;
        }
    }

    void generateBasicAuthentication()
    {
        std::string credentials{c->m_username};
        credentials += ":" + c->m_password;
        m_request.set(boost::beast::http::field::authorization,
                      "Basic " + base64::encode(credentials));
    }

#ifdef ENABLE_DIGEST
    bool generateDigestAuth(boost::string_view authenticate, boost::string_view body)
    {
        digest_authenticator authenticator(authenticate, c->m_username, c->m_password,
                                           m_request.target(), to_string(m_request.method()), body);
        if (authenticator.generateAuthorization()) {
            m_request.set(boost::beast::http::field::authorization, authenticator.authorization());
            return true;
        }
        return false;
    }
#endif

    virtual void connectSocket() = 0;
    virtual void sendRequest() = 0;
    virtual void initiateReadHeader() = 0;
    virtual void initiateRead() = 0;
    virtual void closeSocket(boost::system::error_code& ec) = 0;

    boost::beast::flat_buffer m_buffer;
    ResponseParser m_responseParser;
    boost::beast::http::request<RequestBody> m_request;
    boost::asio::ip::tcp::resolver::results_type m_resolveResults;
    std::shared_ptr<basic_client<RequestBody, ResponseBody>> c;

private:
    virtual std::shared_ptr<client_private> shared_from_this() = 0;

    boost::asio::ip::tcp::resolver m_resolver;
    boost::asio::deadline_timer m_timeout;
    url m_url;
};

} // namespace simple_http

#endif // CLIENT_PRIVATE_HPP
