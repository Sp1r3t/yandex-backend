#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "http_server.h"
#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"

using namespace std::literals;

namespace net = boost::asio;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);

    std::vector<std::thread> workers;
    workers.reserve(n - 1);

    while (--n) {
        workers.emplace_back(fn);
    }

    fn();

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <config-file> <static-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    server_logger::InitLogging();

    try {
        const auto game = json_loader::LoadGame(argv[1]);
        const std::filesystem::path static_root = argv[2];

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            ioc.stop();
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        http_handler::RequestHandler handler{game, static_root};
        http_handler::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        server_logger::LogServerStart(port, address.to_string());

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        server_logger::LogServerExit(EXIT_SUCCESS);
    } catch (const std::exception& ex) {
        const std::string message = ex.what();
        server_logger::LogServerExit(EXIT_FAILURE, &message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}