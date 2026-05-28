#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/program_options.hpp>

#include "application.h"
#include "http_server.h"
#include "json_loader.h"
#include "logger.h"
#include "model_serialization.h"
#include "request_handler.h"

using namespace std::literals;

namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;

namespace {

struct Args {
    std::filesystem::path config_file;
    std::filesystem::path www_root;
    std::optional<std::filesystem::path> state_file;
    std::optional<std::int64_t> save_state_period_ms;
    std::optional<std::int64_t> tick_period_ms;
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
         "path to static files root")
        ("state-file",
         po::value<std::string>()->value_name("file"),
         "path to state save file (optional)")
        ("save-state-period",
         po::value<std::int64_t>()->value_name("ms"),
         "state auto-save period in milliseconds (optional)")
        ("tick-period,t",
         po::value<std::int64_t>()->value_name("ms"),
         "tick period in milliseconds; enables auto-tick mode (optional)");

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

    if (vm.count("state-file")) {
        args.state_file = vm["state-file"].as<std::string>();
    }

    if (vm.count("save-state-period")) {
        args.save_state_period_ms = vm["save-state-period"].as<std::int64_t>();
    }

    if (vm.count("tick-period")) {
        args.tick_period_ms = vm["tick-period"].as<std::int64_t>();
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

        // Restore saved state if state file is specified and exists
        if (args->state_file && std::filesystem::exists(*args->state_file)) {
            serialization::LoadGame(game, *args->state_file);
        }

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        // Subscribe to termination signals
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const bool auto_tick_mode = args->tick_period_ms.has_value();

        // Create application — wraps game + tick + periodic save logic
        app::Application application(game, auto_tick_mode,
                                     args->state_file,
                                     args->save_state_period_ms);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        http_handler::RequestHandler handler{application, extra_data, args->www_root};
        http_handler::LoggingRequestHandler logging_handler{handler};

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        server_logger::LogServerStart(port, address.to_string());

        // Set up auto-tick timer if tick-period is specified
        if (args->tick_period_ms) {
            const auto tick_period = std::chrono::milliseconds(*args->tick_period_ms);

            // Use shared_ptr for both the timer and the recursive callback so
            // they outlive the if-block and live as long as ioc holds them.
            auto timer = std::make_shared<net::steady_timer>(ioc);
            auto tick_fn = std::make_shared<std::function<void()>>();

            *tick_fn = [&application, timer, tick_period, tick_fn]() {
                timer->expires_after(tick_period);
                timer->async_wait(
                    [&application, timer, tick_period, tick_fn](const sys::error_code& ec) {
                        if (!ec) {
                            application.Tick(tick_period.count());
                            (*tick_fn)();
                        }
                    });
            };
            (*tick_fn)();
        }

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        // All async operations are done — save final state before exit
        application.SaveStateOnExit();

        server_logger::LogServerExit(EXIT_SUCCESS);
    } catch (const std::exception& ex) {
        const std::string message = ex.what();
        server_logger::LogServerExit(EXIT_FAILURE, &message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
