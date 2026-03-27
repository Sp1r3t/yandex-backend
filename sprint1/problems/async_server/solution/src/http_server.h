#pragma once
#include "sdk.h"
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

class SessionBase {
public:
    explicit SessionBase(tcp::socket&& socket)
        : stream_(std::move(socket)) {
    }

protected:
    using HttpRequest = http::request<http::string_body>;

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;

    void WriteResponse(http::response<http::string_body>&& response) {
        auto sp = std::make_shared<http::response<http::string_body>>(std::move(response));
        auto self = GetSharedBase();

        http::async_write(stream_, *sp,
                          [self, sp](beast::error_code ec, std::size_t bytes_written) {
                              self->OnWriteDone(ec, bytes_written, sp->need_eof());
                          });
    }

    virtual void Read() = 0;
    virtual void OnWriteDone(beast::error_code ec, std::size_t bytes_written, bool close) = 0;
    virtual std::shared_ptr<SessionBase> GetSharedBase() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    Session(tcp::socket&& socket, RequestHandler& handler)
        : SessionBase(std::move(socket))
        , request_handler_(handler) {
    }

    void Run() {
        net::dispatch(stream_.get_executor(),
                      [self = this->shared_from_this()] {
                          self->Read();
                      });
    }

private:
    struct ResponseSender {
        Session& self;

        template <typename Response>
        void operator()(Response&& response) const {
            self.WriteResponse(std::forward<Response>(response));
        }
    };

    void Read() override {
        request_ = {};

        http::async_read(stream_, buffer_, request_,
                         [self = this->shared_from_this()](beast::error_code ec, std::size_t bytes_read) {
                             self->OnRead(ec, bytes_read);
                         });
    }

    void OnRead(beast::error_code ec, std::size_t) {
        if (ec == http::error::end_of_stream) {
            Close();
            return;
        }
        if (ec) {
            return;
        }

        request_handler_(std::move(request_), ResponseSender{*this});
    }

    void OnWriteDone(beast::error_code ec, std::size_t, bool close) override {
        if (ec) {
            return;
        }

        if (close) {
            Close();
            return;
        }

        Read();
    }

    void Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    std::shared_ptr<SessionBase> GetSharedBase() override {
        return this->shared_from_this();
    }

    RequestHandler& request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler& handler)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , handler_(handler) {
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
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            [self = this->shared_from_this()](beast::error_code ec, tcp::socket socket) {
                self->OnAccept(ec, std::move(socket));
            });
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session<RequestHandler>>(std::move(socket), handler_)->Run();
        }
        DoAccept();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler& handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyHandler = std::decay_t<RequestHandler>;
    auto handler_ptr = std::make_shared<MyHandler>(std::forward<RequestHandler>(handler));
    std::make_shared<Listener<MyHandler>>(ioc, endpoint, *handler_ptr)->Run();
}

}  // namespace http_server