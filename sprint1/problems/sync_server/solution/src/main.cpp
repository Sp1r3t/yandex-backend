#ifdef WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
using namespace std::literals;

http::response<http::string_body> HandleRequest(const http::request<http::string_body>& req) {
    http::response<http::string_body> res;

    if (req.method() == http::verb::get) {
        std::string name(req.target().substr(1));
        res.result(http::status::ok);
        res.version(req.version());
        res.set(http::field::content_type, "text/html");
        res.body() = "Hello, " + name;
    } else {
        res.result(http::status::method_not_allowed);
        res.version(req.version());
        res.set(http::field::content_type, "text/html");
        res.body() = "Invalid method";
    }

    res.content_length(res.body().size());
    res.keep_alive(false);
    return res;
}

void ServeHttp(tcp::socket& socket) {
    beast::flat_buffer buffer;

    http::request<http::string_body> req;
    http::read(socket, buffer, req);

    auto res = HandleRequest(req);
    http::write(socket, res);

    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    try {
        const auto address = net::ip::make_address("0.0.0.0");
        const unsigned short port = 8080;

        net::io_context ioc(1);
        tcp::acceptor acceptor(ioc, {address, port});

        std::cout << "Server started" << std::endl;

        for (;;) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            ServeHttp(socket);
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
