#include "logger.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json/serialize.hpp>
#include <boost/log/attributes/current_process_id.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/keywords/auto_flush.hpp>
#include <boost/log/keywords/format.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#include <iostream>
#include <string>

namespace server_logger {

namespace logging = boost::log;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(message_text, "Message", std::string)

namespace {

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object obj;

    if (const auto ts = rec[timestamp]) {
        obj["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);
    }
    if (const auto data = rec[additional_data]) {
        obj["data"] = *data;
    } else {
        obj["data"] = json::object{};
    }
    if (const auto msg = rec[message_text]) {
        obj["message"] = *msg;
    } else {
        obj["message"] = "";
    }

    strm << json::serialize(obj);
}

}  // namespace

void InitLogging() {
    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true);

    logging::add_common_attributes();
}

void LogEvent(std::string_view message, json::value data) {
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, std::move(data))
                            << message;
}

void LogServerStart(unsigned short port, const std::string& address) {
    LogEvent("server started", json::value{
                                   {"port", port},
                                   {"address", address},
                               });
}

void LogServerExit(int code, const std::string* exception) {
    json::object data;
    data["code"] = code;
    if (exception) {
        data["exception"] = *exception;
    }
    LogEvent("server exited", std::move(data));
}

void LogNetworkError(const beast::error_code& ec, std::string_view where) {
    LogEvent("error", json::value{
                          {"code", ec.value()},
                          {"text", ec.message()},
                          {"where", std::string(where)},
                      });
}

}  // namespace server_logger