#ifndef CLIENT_PRIVATE_HTTP_HPP
#define CLIENT_PRIVATE_HTTP_HPP

#include "client_private.hpp"
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <memory>

namespace simple_http {

template<typename RequestBody, typename ResponseBody>
class basic_client;

template<typename RequestBody, typename ResponseBody>
class client_private_http
  : public client_private<RequestBody, ResponseBody>
  , public std::enable_shared_from_this<client_private_http<RequestBody, ResponseBody>>
{
public:
    explicit client_private_http(boost::asio::io_context& io,
                                 std::shared_ptr<basic_client<RequestBody, ResponseBody>> c)
      : client_private<RequestBody, ResponseBody>{io, c}, m_socket{io}
    {}

private:
    boost::asio::ip::tcp::socket m_socket;

    std::shared_ptr<client_private<RequestBody, ResponseBody>> shared_from_this() override
    {
        return std::enable_shared_from_this<
            client_private_http<RequestBody, ResponseBody>>::shared_from_this();
    }

    std::shared_ptr<client_private_http<RequestBody, ResponseBody>> self()
    {
        return std::enable_shared_from_this<
            client_private_http<RequestBody, ResponseBody>>::shared_from_this();
    }

    void connectSocket()
    {
        this->resetTimeout(Connect);
        boost::asio::async_connect(
            m_socket, this->m_resolveResults.begin(), this->m_resolveResults.end(),
            std::bind(&client_private_http<RequestBody, ResponseBody>::onConnect, self(),
                      std::placeholders::_1));
    }

    void sendRequest()
    {
        this->resetTimeout(RequestSend);
        this->clearResponse();
        boost::beast::http::async_write(
            m_socket, this->m_request,
            std::bind(&client_private_http<RequestBody, ResponseBody>::onWrite, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void initiateReadHeader()
    {
        this->resetTimeout(Header);
        // Receive the HTTP response header
        boost::beast::http::async_read_header(
            m_socket, this->m_buffer, this->m_responseParser,
            std::bind(&client_private_http<RequestBody, ResponseBody>::onReadHeader, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void initiateRead()
    {
        this->resetTimeout(Contents);
        // Receive the HTTP response
        boost::beast::http::async_read(
            m_socket, this->m_buffer, this->m_responseParser,
            std::bind(&client_private_http<RequestBody, ResponseBody>::onRead, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void closeSocket(boost::system::error_code& ec)
    {
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    }
};

} // namespace simple_http

#endif // CLIENT_PRIVATE_HTTP_HPP
