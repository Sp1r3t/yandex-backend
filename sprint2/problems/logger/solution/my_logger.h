#pragma once

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    static std::tm ToLocalTm(std::chrono::system_clock::time_point tp) {
        const auto t_c = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        return tm;
    }

    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    std::string GetTimeStamp(std::chrono::system_clock::time_point tp) const {
        std::ostringstream out;
        const auto tm = ToLocalTm(tp);
        out << std::put_time(&tm, "%F %T");
        return out.str();
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp(std::chrono::system_clock::time_point tp) const {
        std::ostringstream out;
        const auto tm = ToLocalTm(tp);
        out << std::put_time(&tm, "%Y_%m_%d");
        return out.str();
    }

    void OpenLogFileIfNeeded(const std::string& file_stamp) {
        if (current_file_stamp_ == file_stamp && stream_.is_open()) {
            return;
        }

        stream_.close();
        const std::string path = "/var/log/sample_log_" + file_stamp + ".log";
        stream_.open(path, std::ios::app);
        current_file_stamp_ = file_stamp;
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    // Выведите в поток все аргументы.
    template <class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard guard(mutex_);

        const auto now = GetTime();
        OpenLogFileIfNeeded(GetFileTimeStamp(now));

        stream_ << GetTimeStamp(now) << ": ";
        (stream_ << ... << args);
        stream_ << '\n';
        stream_.flush();
    }

    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard guard(mutex_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::mutex mutex_;
    std::ofstream stream_;
    std::string current_file_stamp_;
};
