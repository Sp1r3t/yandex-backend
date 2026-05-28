#pragma once

#include <filesystem>
#include <mutex>
#include <optional>

#include "model.h"
#include "model_serialization.h"

namespace app {

class Application {
public:
    Application(model::Game& game,
                bool auto_tick_mode,
                std::optional<std::filesystem::path> state_file,
                std::optional<std::int64_t> save_period_ms)
        : game_(game)
        , auto_tick_mode_(auto_tick_mode)
        , state_file_(std::move(state_file))
        , save_period_ms_(save_period_ms) {
    }

    // Advance game time and conditionally save state. Called from timer (auto-tick)
    // or request handler (manual tick).
    void Tick(std::int64_t delta_ms) {
        game_.Tick(delta_ms);

        if (save_period_ms_ && state_file_) {
            std::lock_guard lock(save_mutex_);
            time_since_save_ms_ += delta_ms;
            if (time_since_save_ms_ >= *save_period_ms_) {
                DoSave();
                time_since_save_ms_ = 0;
            }
        }
    }

    // Save state unconditionally. Called on shutdown if a state file is configured.
    void SaveStateOnExit() {
        if (state_file_) {
            std::lock_guard lock(save_mutex_);
            DoSave();
        }
    }

    bool IsAutoTickMode() const noexcept {
        return auto_tick_mode_;
    }

    const model::Game& GetGame() const noexcept {
        return game_;
    }

    model::Game& GetGame() noexcept {
        return game_;
    }

private:
    void DoSave() {
        // Caller holds save_mutex_
        serialization::SaveGame(game_, *state_file_);
    }

    model::Game& game_;
    bool auto_tick_mode_;
    std::optional<std::filesystem::path> state_file_;
    std::optional<std::int64_t> save_period_ms_;

    std::mutex save_mutex_;
    std::int64_t time_since_save_ms_ = 0;
};

}  // namespace app
