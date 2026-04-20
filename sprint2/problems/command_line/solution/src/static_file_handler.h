#pragma once

#include <boost/beast/http.hpp>
#include <filesystem>
#include <string>
#include <string_view>

namespace http_handler {

namespace http = boost::beast::http;

class StaticFileHandler {
public:
    explicit StaticFileHandler(std::filesystem::path root);

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleRequest(
        const http::request<Body, http::basic_fields<Allocator>>& req) const;

private:
    static std::string UrlDecode(std::string_view value);
    static std::string GetMimeType(std::string_view path);
    static bool IsSubPath(const std::filesystem::path& path, const std::filesystem::path& base);

    std::filesystem::path root_;
};

}  // namespace http_handler