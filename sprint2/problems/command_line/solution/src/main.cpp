#include <algorithm>
#include <chrono>
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
#include <boost/asio/strand.hpp>
#include <boost/program_options.hpp>

#include "http_server.h"
#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"

using namespace std::literals;

namespace net = boost::asio;
namespace po = boost::program_options;
namespace sys = boost::system;

namespace {

struct CommandLineArgs {
    std::filesystem::path config_file;
    std::filesystem::path www_root;
    std::optional<std::chrono::milliseconds> tick_period;
    bool randomize_spawn_points = false;
};

// Таймер, периодически обновляющий игровое состояние внутри strand.
// Между тиками измеряется реальное прошедшее время, а не номинальный период.
class AutoTicker : public std::enable_shared_from_this<AutoTicker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    AutoTicker(Strand strand,
               std::shared_ptr<model::Game> game,
               std::chrono::milliseconds tick_period)
        : strand_(strand)
        , timer_(strand_)
        , game_(std::move(game))
        , tick_period_(tick_period) {
    }

    void Start() {
        last_tick_ = Clock::now();
        net::dispatch(strand_, [self = shared_from_this()] {
            self->ScheduleNextTick();
        });
    }

private:
    using Clock = std::chrono::steady_clock;

    void ScheduleNextTick() {
        timer_.expires_after(tick_period_);
        timer_.async_wait([self = shared_from_this()](const sys::error_code& ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(const sys::error_code& ec) {
        if (ec) {
            return;
        }

        const auto now = Clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_);
        last_tick_ = now;

        try {
            game_->Tick(delta.count());
        } catch (...) {
            // Ошибки обработчика не должны ломать цикл таймера.
        }

        ScheduleNextTick();
    }

    Strand strand_;
    net::steady_timer timer_;
    std::shared_ptr<model::Game> game_;
    std::chrono::milliseconds tick_period_;
    Clock::time_point last_tick_;
};

po::options_description MakeCommandLineDescription() {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t",
         po::value<int>()->value_name("milliseconds"),
         "set tick period")
        ("config-file,c",
         po::value<std::string>()->value_name("file"),
         "set config file path")
        ("www-root,w",
         po::value<std::string>()->value_name("dir"),
         "set static files root")
        ("randomize-spawn-points",
         "spawn dogs at random positions");
    return desc;
}

std::optional<CommandLineArgs> ParseCommandLine(int argc,
                                                const char* argv[],
                                                std::ostream& out,
                                                std::ostream& err,
                                                bool& help_requested) {
    help_requested = false;

    const auto desc = MakeCommandLineDescription();

    po::variables_map vm;

    try {
        auto parser = po::command_line_parser(argc, argv);
        parser.options(desc);
        po::store(parser.run(), vm);
        po::notify(vm);
    } catch (const po::error& ex) {
        err << ex.what() << '\n' << desc << std::endl;
        return std::nullopt;
    }

    if (vm.count("help")) {
        help_requested = true;
        out << desc << std::endl;
        return CommandLineArgs{};
    }

    if (!vm.count("config-file")) {
        err << "Config file path is not specified" << '\n' << desc << std::endl;
        return std::nullopt;
    }

    if (!vm.count("www-root")) {
        err << "Static files root is not specified" << '\n' << desc << std::endl;
        return std::nullopt;
    }

    CommandLineArgs args;
    args.config_file = vm["config-file"].as<std::string>();
    args.www_root = vm["www-root"].as<std::string>();
    args.randomize_spawn_points = vm.count("randomize-spawn-points") != 0;

    if (vm.count("tick-period")) {
        const int tick_period_ms = vm["tick-period"].as<int>();
        if (tick_period_ms <= 0) {
            err << "Tick period must be positive" << std::endl;
            return std::nullopt;
        }
        args.tick_period = std::chrono::milliseconds(tick_period_ms);
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
    bool help_requested = false;
    const auto args = ParseCommandLine(argc, argv, std::cout, std::cerr, help_requested);
    if (!args) {
        return EXIT_FAILURE;
    }
    if (help_requested) {
        return EXIT_SUCCESS;
    }

    server_logger::InitLogging();

    try {
        auto game = std::make_shared<model::Game>(json_loader::LoadGame(args->config_file));
        game->SetRandomizeSpawnPoints(args->randomize_spawn_points);

        const std::filesystem::path static_root = args->www_root;
        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code&, int) {
            ioc.stop();
        });

        // strand, в котором выполняются обращения к игровому состоянию из таймера.
        auto api_strand = net::make_strand(ioc);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        if (args->tick_period) {
            std::make_shared<AutoTicker>(api_strand, game, *args->tick_period)->Start();
        }

        http_handler::RequestHandler handler{*game, static_root, !args->tick_period.has_value()};
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
