#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include "http_server.h"
#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"

using namespace std::literals;

namespace net = boost::asio;
namespace po = boost::program_options;

namespace {

struct Args {
    std::filesystem::path config_file;
    std::filesystem::path www_root;
};

std::optional<Args> ParseCommandLine(int argc, const char* argv[]) {
    po::options_description desc("Game server options");
    desc.add_options()
        ("help,h", "produce help message")
        ("config-file,c",
         po::value<std::string>()->value_name("file"),
         "path to config file")
        ("www-root,w",
         po::value<std::string>()->value_name("dir"),
         "path to static files root");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& ex) {
        std::cerr << ex.what() << "\n" << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    Args args;

    if (vm.count("config-file")) {
        args.config_file = vm["config-file"].as<std::string>();
    } else {
        std::cerr << "Config file path is not specified\n" << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("www-root")) {
        args.www_root = vm["www-root"].as<std::string>();
    } else {
        std::cerr << "Static files root is not specified\n" << desc << std::endl;
        return std::nullopt;
    }

    return args;
}

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
    const auto args = ParseCommandLine(argc, argv);
    if (!args) {
        return EXIT_FAILURE;
    }

    server_logger::InitLogging();

    try {
        auto [game, extra_data] = json_loader::LoadGame(args->config_file);

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code&, int) {
            ioc.stop();
        });

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        http_handler::RequestHandler handler{game, extra_data, args->www_root};
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
