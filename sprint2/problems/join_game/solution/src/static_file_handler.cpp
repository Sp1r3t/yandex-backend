#include "static_file_handler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace http_handler {

using namespace std::literals;

StaticFileHandler::StaticFileHandler(std::filesystem::path root)
    : root_(std::filesystem::weakly_canonical(std::move(root))) {
}

std::string StaticFileHandler::UrlDecode(std::string_view value) {
    std::string result;
    result.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const auto hex_to_int = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                return -1;
            };

            int hi = hex_to_int(value[i + 1]);
            int lo = hex_to_int(value[i + 2]);
            if (hi == -1 || lo == -1) {
                throw std::runtime_error("Invalid URL encoding");
            }

            result.push_back(static_cast<char>(hi * 16 + lo));
            i += 2;
        } else if (value[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(value[i]);
        }
    }

    return result;
}

std::string StaticFileHandler::GetMimeType(std::string_view path) {
    namespace fs = std::filesystem;

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".htm" || ext == ".html") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".txt") return "text/plain";
    if (ext == ".js") return "text/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpe" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".ico") return "image/vnd.microsoft.icon";
    if (ext == ".tiff" || ext == ".tif") return "image/tiff";
    if (ext == ".svg" || ext == ".svgz") return "image/svg+xml";
    if (ext == ".mp3") return "audio/mpeg";

    return "application/octet-stream";
}

bool StaticFileHandler::IsSubPath(const std::filesystem::path& path,
                                  const std::filesystem::path& base) {
    auto canonical_path = std::filesystem::weakly_canonical(path);
    auto canonical_base = std::filesystem::weakly_canonical(base);

    auto path_it = canonical_path.begin();
    auto base_it = canonical_base.begin();

    for (; base_it != canonical_base.end(); ++base_it, ++path_it) {
        if (path_it == canonical_path.end() || *path_it != *base_it) {
            return false;
        }
    }
    return true;
}

template <typename Body, typename Allocator>
http::response<http::string_body> StaticFileHandler::HandleRequest(
    const http::request<Body, http::basic_fields<Allocator>>& req) const {

    auto make_plain_response = [&](http::status status, std::string body, std::string_view allow = {}) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "text/plain");
        if (!allow.empty()) {
            res.set(http::field::allow, allow);
        }
        res.body() = std::move(body);
        res.content_length(res.body().size());
        res.keep_alive(req.keep_alive());
        return res;
    };

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        return make_plain_response(http::status::method_not_allowed, "Invalid method", "GET, HEAD");
    }

    std::string decoded_path;
    try {
        decoded_path = UrlDecode(std::string(req.target()));
    } catch (...) {
        return make_plain_response(http::status::bad_request, "Bad request");
    }

    if (decoded_path.empty() || decoded_path[0] != '/') {
        return make_plain_response(http::status::bad_request, "Bad request");
    }

    std::filesystem::path relative = decoded_path.substr(1);
    std::filesystem::path file_path = root_ / relative;

    if (decoded_path.back() == '/' || std::filesystem::is_directory(file_path)) {
        file_path /= "index.html";
    }

    file_path = std::filesystem::weakly_canonical(file_path);

    if (!IsSubPath(file_path, root_)) {
        return make_plain_response(http::status::bad_request, "Bad request");
    }

    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        return make_plain_response(http::status::not_found, "File not found");
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return make_plain_response(http::status::not_found, "File not found");
    }

    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, GetMimeType(file_path.string()));
    res.content_length(body.size());
    res.keep_alive(req.keep_alive());

    if (req.method() != http::verb::head) {
        res.body() = std::move(body);
    }

    return res;
}

template http::response<http::string_body>
StaticFileHandler::HandleRequest(
    const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) const;

}  // namespace http_handler