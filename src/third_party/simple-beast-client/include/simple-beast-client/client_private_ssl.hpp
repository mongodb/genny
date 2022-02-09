#ifndef CLIENT_PRIVATE_SSL_HPP
#define CLIENT_PRIVATE_SSL_HPP

#include "client_private.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <memory>
#if BOOST_OS_WINDOWS
#include <wincrypt.h>
#endif

namespace simple_http {

template<typename RequestBody, typename ResponseBody>
class basic_client;

class SslContextManager
{
public:
    SslContextManager()
    {
        namespace ssl = boost::asio::ssl;
        m_ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                          ssl::context::no_sslv3 | ssl::context::tlsv12_client);
#if BOOST_OS_WINDOWS
        addWindowsRootCerts();
#else
        m_ctx.set_default_verify_paths();
#endif
    }

    auto& ctx() { return m_ctx; }

private:
#if BOOST_OS_WINDOWS
    void addWindowsRootCerts()
    {
        HCERTSTORE hStore = CertOpenSystemStoreA(0, "ROOT");
        if (hStore == NULL) {
            return;
        }

        X509_STORE* store = X509_STORE_new();
        PCCERT_CONTEXT pContext = NULL;
        while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
            X509* x509 = d2i_X509(NULL, (const unsigned char**)&pContext->pbCertEncoded,
                                  pContext->cbCertEncoded);
            if (x509 != NULL) {
                X509_STORE_add_cert(store, x509);
                X509_free(x509);
            }
        }

        CertFreeCertificateContext(pContext);
        CertCloseStore(hStore, 0);

        SSL_CTX_set_cert_store(m_ctx.native_handle(), store);
    }
#endif

    boost::asio::ssl::context m_ctx{boost::asio::ssl::context::tlsv12_client};
};

inline boost::asio::ssl::context& ssl_context_g()
{
    static SslContextManager s{};
    return s.ctx();
}

template<typename RequestBody, typename ResponseBody>
class client_private_ssl
  : public client_private<RequestBody, ResponseBody>
  , public std::enable_shared_from_this<client_private_ssl<RequestBody, ResponseBody>>
{
public:
    explicit client_private_ssl(boost::asio::io_context& io,
                                std::shared_ptr<basic_client<RequestBody, ResponseBody>> cl)
      : client_private<RequestBody, ResponseBody>{io, cl}, m_stream{io, ssl_context_g()}
    {}

private:
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_stream;

    std::shared_ptr<client_private<RequestBody, ResponseBody>> shared_from_this()
    {
        return std::enable_shared_from_this<
            client_private_ssl<RequestBody, ResponseBody>>::shared_from_this();
    }

    std::shared_ptr<client_private_ssl<RequestBody, ResponseBody>> self()
    {
        return std::enable_shared_from_this<
            client_private_ssl<RequestBody, ResponseBody>>::shared_from_this();
    }

    void connectSocket()
    {
        this->resetTimeout(Connect);
        boost::asio::async_connect(
            m_stream.next_layer(), this->m_resolveResults.begin(), this->m_resolveResults.end(),
            std::bind(&client_private_ssl<RequestBody, ResponseBody>::onSslConnect, self(),
                      std::placeholders::_1));
    }

    void onSslConnect(boost::system::error_code ec)
    {
        if (ec) {
            this->fail(ConnectionError, "Error connecting: " + ec.message());
            return;
        }
        // Perform the SSL handshake
        m_stream.async_handshake(
            boost::asio::ssl::stream_base::client,
            std::bind(&client_private_ssl<RequestBody, ResponseBody>::onHandshake, self(),
                      std::placeholders::_1));
    }

    void onHandshake(boost::system::error_code ec)
    {
        if (ec) {
            this->fail(HandshakeError, "Error during handshake: " + ec.message());
            return;
        }
        sendRequest();
    }

    void sendRequest()
    {
        this->resetTimeout(RequestSend);
        this->clearResponse();
        // Send the HTTP request to the remote host
        boost::beast::http::async_write(
            m_stream, this->m_request,
            std::bind(&client_private_ssl<RequestBody, ResponseBody>::onWrite, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void initiateReadHeader()
    {
        this->resetTimeout(Header);
        // Receive the HTTP response
        boost::beast::http::async_read_header(
            m_stream, this->m_buffer, this->m_responseParser,
            std::bind(&client_private_ssl<RequestBody, ResponseBody>::onReadHeader, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void initiateRead()
    {
        this->resetTimeout(Contents);
        // Receive the HTTP response
        boost::beast::http::async_read(
            m_stream, this->m_buffer, this->m_responseParser,
            std::bind(&client_private_ssl<RequestBody, ResponseBody>::onRead, self(),
                      std::placeholders::_1, std::placeholders::_2));
    }

    void closeSocket(boost::system::error_code& ec)
    {
        // Gracefully close the stream
        m_stream.async_shutdown(
            std::bind(&client_private_ssl::onShutdown, self(), std::placeholders::_1));
    }

    void onShutdown(boost::system::error_code ec)
    {
        if (ec && ec != boost::asio::error::eof) {
            this->c->failure(Unknown, "Unexpected error on shutdown: " + ec.message());
        }
        // If we get here then the connection is closed gracefully
    }
};

} // namespace simple_http

#endif // CLIENT_PRIVATE_SSL_HPP
