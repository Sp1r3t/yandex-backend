#pragma once

#include "sdk.h"

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace http_server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

template <class Stream>
struct SendLambda {
    Stream& stream;
    bool& close;
    beast::error_code& ec;

    explicit SendLambda(Stream& stream, bool& close, beast::error_code& ec)
        : stream(stream)
        , close(close)
        , ec(ec) {
    }

    template <bool isRequest, class Body, class Fields>
    void operator()(http::message<isRequest, Body, Fields>&& msg) const {
        close = msg.need_eof();
        auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));
        http::write(stream, *sp, ec);
    }
};

template <typename RequestHandler>
class Session : public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    Session(tcp::socket&& socket, RequestHandler handler)
        : stream_(std::move(socket))
        , request_handler_(std::move(handler)) {
    }

    void Run() {
        net::dispatch(stream_.get_executor(), beast::bind_front_handler(&Session::Read, this->shared_from_this()));
    }

private:
    void Read() {
        req_ = {};
        http::async_read(stream_, buffer_, req_, beast::bind_front_handler(&Session::OnRead, this->shared_from_this()));
    }

    void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return;
        }

        SendLambda<beast::tcp_stream> lambda{stream_, close_, ec_};
        request_handler_(std::move(req_), lambda);
        if (ec_) {
            return;
        }
        if (close_) {
            return Close();
        }
        Read();
    }

    void Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    RequestHandler request_handler_;
    bool close_ = false;
    beast::error_code ec_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, tcp::endpoint endpoint, RequestHandler handler)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::move(handler)) {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            throw beast::system_error(ec);
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            throw beast::system_error(ec);
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            throw beast::system_error(ec);
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            throw beast::system_error(ec);
        }
    }

    void Run() {
        Accept();
    }

private:
    void Accept() {
        acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
        }
        Accept();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler handler) {
    std::make_shared<Listener<RequestHandler>>(ioc, endpoint, std::move(handler))->Run();
}

}  // namespace http_server
