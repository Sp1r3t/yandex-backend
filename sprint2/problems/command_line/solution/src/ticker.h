#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <memory>

#include "model.h"

namespace ticker {

namespace net = boost::asio;
namespace sys = boost::system;

class AutoTicker : public std::enable_shared_from_this<AutoTicker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    AutoTicker(Strand strand,
               std::shared_ptr<model::Game> game,
               std::chrono::milliseconds tick_period)
        : strand_(std::move(strand))
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
        }

        ScheduleNextTick();
    }

    Strand strand_;
    net::steady_timer timer_;
    std::shared_ptr<model::Game> game_;
    std::chrono::milliseconds tick_period_;
    Clock::time_point last_tick_;
};

}  // namespace ticker
