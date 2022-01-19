// Copyright 2021-present MongoDB Inc.
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

#ifndef HEADER_7A9C2F9E_ECCA_4AC9_AF39_8EE877217977_INCLUDED
#define HEADER_7A9C2F9E_ECCA_4AC9_AF39_8EE877217977_INCLUDED

#include <string>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace genny::actor::http {

boost::beast::ssl_stream<boost::beast::tcp_stream> make_tls_stream(std::string host, std::string port) {
    namespace beast = boost::beast; // from <boost/beast.hpp>
    namespace http = beast::http;   // from <boost/beast/http.hpp>
    namespace net = boost::asio;    // from <boost/asio.hpp>
    namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
    using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx(ssl::context::tlsv11);

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    // This does not work on Windows. If we ever care,
    // see https://stackoverflow.com/a/40710806
#error Windows is not supported by this function
#else
    ctx.set_default_verify_paths();
#endif

    // These objects perform our I/O
    tcp::resolver resolver(ioc);
    beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
    {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        BOOST_THROW_EXCEPTION(beast::system_error{ec});
    }

    // Look up the domain name
    auto const results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(stream).connect(results);

    // Perform the SSL handshake
    stream.handshake(ssl::stream_base::client);
    stream.set_verify_mode(ssl::verify_peer);

    return stream;
}

boost::beast::ssl_stream<boost::beast::tcp_stream> make_tls_stream(std::string host, int port) {
    if(port <= 0 || port > 65535) {
        throw std::runtime_error("port number out of range. Must be 0 < port < 65536");
    }
    return make_tls_stream(host, std::to_string(port));
}
}  // namespace genny::actor

#endif  // HEADER_7A9C2F9E_ECCA_4AC9_AF39_8EE877217977_INCLUDED
