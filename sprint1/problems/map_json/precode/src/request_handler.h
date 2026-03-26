#pragma once

#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(const model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) const {
        using StringResponse = http::response<http::string_body>;

        const auto make_json_response = [&req](http::status status, std::string body) {
            StringResponse res{status, req.version()};
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

        if (req.method() != http::verb::get) {
            return send(make_error(http::status::bad_request, "badRequest", "Bad request"));
        }

        const std::string target = std::string(req.target());
        if (!target.starts_with("/api/")) {
            return send(make_error(http::status::bad_request, "badRequest", "Bad request"));
        }

        if (target == "/api/v1/maps") {
            json::array maps;
            for (const auto& map : game_.GetMaps()) {
                json::object item;
                item["id"] = *map.GetId();
                item["name"] = map.GetName();
                maps.emplace_back(std::move(item));
            }
            return send(make_json_response(http::status::ok, json::serialize(maps)));
        }

        constexpr std::string_view maps_prefix = "/api/v1/maps/";
        if (target.starts_with(maps_prefix)) {
            const std::string map_id = target.substr(maps_prefix.size());
            if (map_id.empty() || map_id.find('/') != std::string::npos) {
                return send(make_error(http::status::bad_request, "badRequest", "Bad request"));
            }
            if (const auto* map = game_.FindMap(model::Map::Id(map_id))) {
                return send(make_json_response(http::status::ok, json::serialize(MapToJson(*map))));
            }
            return send(make_error(http::status::not_found, "mapNotFound", "Map not found"));
        }

        return send(make_error(http::status::bad_request, "badRequest", "Bad request"));
    }

private:
    static json::object RoadToJson(const model::Road& road);
    static json::object BuildingToJson(const model::Building& building);
    static json::object OfficeToJson(const model::Office& office);
    static json::object MapToJson(const model::Map& map);

    const model::Game& game_;
};

}  // namespace http_handler
