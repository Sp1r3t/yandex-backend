#pragma once

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <string>
#include <string_view>

#include "model.h"
#include "static_file_handler.h"

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    RequestHandler(const model::Game& game, std::filesystem::path static_root)
        : game_(game)
        , static_handler_(std::move(static_root)) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) const {
        const auto make_json_response = [&](http::status status, std::string body) {
            http::response<http::string_body> res{status, req.version()};
            res.set(http::field::content_type, "application/json");
            res.body() = std::move(body);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            return res;
        };

        const auto make_error = [&](http::status status, std::string code, std::string message) {
            json::object obj;
            obj["code"] = std::move(code);
            obj["message"] = std::move(message);
            return make_json_response(status, json::serialize(obj));
        };

        const std::string target = std::string(req.target());

        if (target.rfind("/api/", 0) != 0) {
            send(static_handler_.HandleRequest(req));
            return;
        }

        if (req.method() != http::verb::get) {
            send(make_error(http::status::bad_request, "badRequest", "Bad request"));
            return;
        }

        if (target == "/api/v1/maps") {
            json::array maps;
            for (const auto& map : game_.GetMaps()) {
                json::object item;
                item["id"] = *map.GetId();
                item["name"] = map.GetName();
                maps.emplace_back(std::move(item));
            }
            send(make_json_response(http::status::ok, json::serialize(maps)));
            return;
        }

        constexpr std::string_view maps_prefix = "/api/v1/maps/";
        if (target.rfind(std::string(maps_prefix), 0) == 0) {
            const std::string map_id = target.substr(maps_prefix.size());

            if (map_id.empty() || map_id.find('/') != std::string::npos) {
                send(make_error(http::status::bad_request, "badRequest", "Bad request"));
                return;
            }

            if (const auto* map = game_.FindMap(model::Map::Id(map_id))) {
                send(make_json_response(http::status::ok, json::serialize(MapToJson(*map))));
                return;
            }

            send(make_error(http::status::not_found, "mapNotFound", "Map not found"));
            return;
        }

        send(make_error(http::status::bad_request, "badRequest", "Bad request"));
    }

private:
    static json::object RoadToJson(const model::Road& road);
    static json::object BuildingToJson(const model::Building& building);
    static json::object OfficeToJson(const model::Office& office);
    static json::object MapToJson(const model::Map& map);

    const model::Game& game_;
    StaticFileHandler static_handler_;
};

}  // namespace http_handler