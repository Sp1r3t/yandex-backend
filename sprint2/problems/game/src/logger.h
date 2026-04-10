#pragma once

#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <boost/json/value.hpp>

#include <string>
#include <string_view>

namespace server_logger {

namespace json = boost::json;
namespace beast = boost::beast;

void InitLogging();

void LogServerStart(unsigned short port, const std::string& address);
void LogServerExit(int code, const std::string* exception = nullptr);
void LogNetworkError(const beast::error_code& ec, std::string_view where);

template <typename Request>
json::value MakeRequestData(const Request& req, const std::string& ip) {
    return json::value{
        {"ip", ip},
        {"URI", std::string(req.target())},
        {"method", std::string(req.method_string())}
    };
}

template <typename Response>
json::value MakeResponseData(const Response& res, long long response_time_ms) {
    json::object data;
    data["response_time"] = response_time_ms;
    data["code"] = res.result_int();

    if (const auto it = res.find(boost::beast::http::field::content_type); it != res.end()) {
        data["content_type"] = std::string(it->value());
    } else {
        data["content_type"] = nullptr;
    }

    return data;
}

void LogEvent(std::string_view message, json::value data);

}  // namespace server_logger