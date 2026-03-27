#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <mutex>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        try {
            auto [bread, sausage, hotdog_id] = TakeIngredients();
            std::make_shared<OrderState>(
                io_, gas_cooker_, std::move(bread), std::move(sausage), hotdog_id, std::move(handler))
                ->Start();
        } catch (...) {
            std::move(handler)(Result<HotDog>::FromCurrentException());
        }
    }

private:
    struct OrderBundle {
        std::shared_ptr<Bread> bread;
        std::shared_ptr<Sausage> sausage;
        int hotdog_id;
    };

    class OrderState : public std::enable_shared_from_this<OrderState> {
    public:
        using Strand = net::strand<net::io_context::executor_type>;

        OrderState(net::io_context& io,
                   std::shared_ptr<GasCooker> cooker,
                   std::shared_ptr<Bread> bread,
                   std::shared_ptr<Sausage> sausage,
                   int hotdog_id,
                   HotDogHandler handler)
            : strand_{net::make_strand(io)}
            , bread_timer_{strand_}
            , sausage_timer_{strand_}
            , cooker_{std::move(cooker)}
            , bread_{std::move(bread)}
            , sausage_{std::move(sausage)}
            , hotdog_id_{hotdog_id}
            , handler_{std::move(handler)} {
        }

        void Start() {
            try {
                bread_->StartBake(*cooker_, net::bind_executor(strand_, [self = this->shared_from_this()] {
                    self->OnBreadStarted();
                }));

                sausage_->StartFry(*cooker_, net::bind_executor(strand_, [self = this->shared_from_this()] {
                    self->OnSausageStarted();
                }));
            } catch (...) {
                CompleteWithError(std::current_exception());
            }
        }

    private:
        void OnBreadStarted() {
            bread_timer_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
            bread_timer_.async_wait([self = this->shared_from_this()](sys::error_code ec) {
                if (ec) {
                    return;
                }
                try {
                    self->bread_->StopBaking();
                    self->OnIngredientReady();
                } catch (...) {
                    self->CompleteWithError(std::current_exception());
                }
            });
        }

        void OnSausageStarted() {
            sausage_timer_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
            sausage_timer_.async_wait([self = this->shared_from_this()](sys::error_code ec) {
                if (ec) {
                    return;
                }
                try {
                    self->sausage_->StopFry();
                    self->OnIngredientReady();
                } catch (...) {
                    self->CompleteWithError(std::current_exception());
                }
            });
        }

        void OnIngredientReady() {
            if (completed_) {
                return;
            }

            ++ready_ingredients_;
            if (ready_ingredients_ != 2) {
                return;
            }

            try {
                CompleteSuccessfully(HotDog{hotdog_id_, sausage_, bread_});
            } catch (...) {
                CompleteWithError(std::current_exception());
            }
        }

        void CompleteSuccessfully(HotDog hotdog) {
            if (completed_) {
                return;
            }
            completed_ = true;
            std::move(handler_)(Result<HotDog>{std::move(hotdog)});
        }

        void CompleteWithError(std::exception_ptr error) {
            if (completed_) {
                return;
            }
            completed_ = true;
            std::move(handler_)(Result<HotDog>{error});
        }

    private:
        Strand strand_;
        net::steady_timer bread_timer_;
        net::steady_timer sausage_timer_;
        std::shared_ptr<GasCooker> cooker_;
        std::shared_ptr<Bread> bread_;
        std::shared_ptr<Sausage> sausage_;
        int hotdog_id_;
        HotDogHandler handler_;
        int ready_ingredients_ = 0;
        bool completed_ = false;
    };

    OrderBundle TakeIngredients() {
        std::lock_guard lk{mutex_};
        return OrderBundle{store_.GetBread(), store_.GetSausage(), ++next_hotdog_id_};
    }

private:
    net::io_context& io_;

    // Используется для создания ингредиентов хот-дога
    Store store_;
    std::mutex mutex_;
    int next_hotdog_id_ = 0;

    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
