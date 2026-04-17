#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "logger.h"
#include "model.h"
#include "static_file_handler.h"

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;
using tcp = boost::asio::ip::tcp;

class RequestHandler {
public:
    RequestHandler(const model::Game& game, std::filesystem::path static_root)
        : game_(game)
        , static_handler_(std::move(static_root)) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    [[maybe_unused]] const tcp::endpoint& remote_endpoint,
                    Send&& send) const {
        const auto send_response = [&](http::status status,
                                       std::string body,
                                       std::string_view content_type,
                                       std::optional<std::string_view> allow = std::nullopt) {
            if (req.method() == http::verb::head) {
                http::response<http::empty_body> res{status, req.version()};
                res.set(http::field::content_type, std::string(content_type));
                res.set(http::field::cache_control, "no-cache");
                if (allow) {
                    res.set(http::field::allow, std::string(*allow));
                }
                res.content_length(body.size());
                res.keep_alive(req.keep_alive());
                send(std::move(res));
                return;
            }

            http::response<http::string_body> res{status, req.version()};
            res.set(http::field::content_type, std::string(content_type));
            res.set(http::field::cache_control, "no-cache");
            if (allow) {
                res.set(http::field::allow, std::string(*allow));
            }
            res.body() = std::move(body);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            send(std::move(res));
        };

        const auto send_json = [&](http::status status,
                                   const json::value& value,
                                   std::optional<std::string_view> allow = std::nullopt) {
            send_response(status, json::serialize(value), "application/json", allow);
        };

        const auto send_error = [&](http::status status,
                                    std::string code,
                                    std::string message,
                                    std::optional<std::string_view> allow = std::nullopt) {
            json::object obj;
            obj["code"] = std::move(code);
            obj["message"] = std::move(message);
            send_json(status, obj, allow);
        };

        const auto handle_authorized_get = [&](auto&& body_builder) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                send_error(http::status::method_not_allowed,
                           "invalidMethod",
                           "Invalid method",
                           "GET, HEAD");
                return;
            }

            const auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                send_error(http::status::unauthorized,
                           "invalidToken",
                           "Authorization header is required");
                return;
            }

            const auto auth_value = auth_header->value();
            const auto token = ExtractBearerToken(
                std::string_view(auth_value.data(), auth_value.size())
            );
            if (!token) {
                send_error(http::status::unauthorized,
                           "invalidToken",
                           "Authorization header is invalid");
                return;
            }

            body_builder(*token);
        };

        const std::string target = std::string(req.target());
        if (target.rfind("/api/", 0) != 0) {
            send(static_handler_.HandleRequest(req));
            return;
        }

        if (target == "/api/v1/game/join") {
            if (req.method() != http::verb::post) {
                send_error(http::status::method_not_allowed,
                           "invalidMethod",
                           "Only POST method is expected",
                           "POST");
                return;
            }

            if (!IsApplicationJson(req)) {
                send_error(http::status::bad_request,
                           "invalidArgument",
                           "Invalid content type");
                return;
            }

            try {
                const json::value parsed = json::parse(req.body());
                const json::object& obj = parsed.as_object();
                const std::string user_name = std::string(obj.at("userName").as_string().c_str());
                const std::string map_id = std::string(obj.at("mapId").as_string().c_str());

                if (user_name.empty()) {
                    send_error(http::status::bad_request,
                               "invalidArgument",
                               "Invalid name");
                    return;
                }

                const auto join_result = game_.JoinPlayer(model::Map::Id(map_id), user_name);
                if (!join_result) {
                    send_error(http::status::not_found,
                               "mapNotFound",
                               "Map not found");
                    return;
                }

                json::object response;
                response["authToken"] = join_result->auth_token;
                response["playerId"] = join_result->player_id;
                send_json(http::status::ok, response);
                return;
            } catch (const std::exception&) {
                send_error(http::status::bad_request,
                           "invalidArgument",
                           "Join game request parse error");
                return;
            }
        }

        if (target == "/api/v1/game/players") {
            handle_authorized_get([&](std::string_view token) {
                const auto player = game_.FindPlayerByToken(token);
                if (!player) {
                    send_error(http::status::unauthorized,
                               "unknownToken",
                               "Player token has not been found");
                    return;
                }

                json::object response;
                for (const auto& other_player : game_.GetPlayersOnMap(player->map_id)) {
                    response[std::to_string(other_player.id)] =
                        json::object{{"name", other_player.name}};
                }
                send_json(http::status::ok, response);
            });
            return;
        }

        if (target == "/api/v1/game/state") {
            handle_authorized_get([&](std::string_view token) {
                const auto snapshots = game_.GetStateByToken(token);
                if (!snapshots) {
                    send_error(http::status::unauthorized,
                               "unknownToken",
                               "Player token has not been found");
                    return;
                }

                json::object players_obj;
                for (const auto& snap : *snapshots) {
                    json::array pos;
                    pos.emplace_back(snap.position.x);
                    pos.emplace_back(snap.position.y);

                    json::array speed;
                    speed.emplace_back(snap.speed.vx);
                    speed.emplace_back(snap.speed.vy);

                    json::object entry;
                    entry["pos"] = std::move(pos);
                    entry["speed"] = std::move(speed);
                    entry["dir"] = std::string(DirectionToString(snap.direction));
                    players_obj[std::to_string(snap.id)] = std::move(entry);
                }

                json::object response;
                response["players"] = std::move(players_obj);
                send_json(http::status::ok, response);
            });
            return;
        }

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send_error(http::status::method_not_allowed,
                       "invalidMethod",
                       "Invalid method",
                       "GET, HEAD");
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
            send_json(http::status::ok, maps);
            return;
        }

        constexpr std::string_view maps_prefix = "/api/v1/maps/";
        if (target.rfind(std::string(maps_prefix), 0) == 0) {
            const std::string map_id = target.substr(maps_prefix.size());

            if (map_id.empty() || map_id.find('/') != std::string::npos) {
                send_error(http::status::bad_request,
                           "badRequest",
                           "Bad request");
                return;
            }

            if (const auto* map = game_.FindMap(model::Map::Id(map_id))) {
                send_json(http::status::ok, MapToJson(*map));
                return;
            }

            send_error(http::status::not_found,
                       "mapNotFound",
                       "Map not found");
            return;
        }

        send_error(http::status::bad_request,
                   "badRequest",
                   "Bad request");
    }

private:
    template <typename Body, typename Allocator>
    static bool IsApplicationJson(const http::request<Body, http::basic_fields<Allocator>>& req) {
        const auto it = req.find(http::field::content_type);
        if (it == req.end()) {
            return false;
        }

        std::string value = std::string(it->value());
        const auto semicolon_pos = value.find(';');
        if (semicolon_pos != std::string::npos) {
            value.erase(semicolon_pos);
        }

        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }

        size_t first_non_space = 0;
        while (first_non_space < value.size() && std::isspace(static_cast<unsigned char>(value[first_non_space]))) {
            ++first_non_space;
        }
        value.erase(0, first_non_space);

        for (char& ch : value) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value == "application/json";
    }

    static std::optional<std::string> ExtractBearerToken(std::string_view auth_header) {
        static constexpr std::string_view kBearerPrefix = "Bearer ";
        if (auth_header.substr(0, kBearerPrefix.size()) != kBearerPrefix) {
            return std::nullopt;
        }

        auth_header.remove_prefix(kBearerPrefix.size());
        if (auth_header.size() != 32) {
            return std::nullopt;
        }

        for (char ch : auth_header) {
            if (!std::isxdigit(static_cast<unsigned char>(ch))) {
                return std::nullopt;
            }
        }

        return std::string(auth_header);
    }

    static std::string_view DirectionToString(model::Direction dir) noexcept {
        switch (dir) {
            case model::Direction::NORTH: return "U";
            case model::Direction::SOUTH: return "D";
            case model::Direction::WEST:  return "L";
            case model::Direction::EAST:  return "R";
        }
        return "U";
    }

    static json::object RoadToJson(const model::Road& road);
    static json::object BuildingToJson(const model::Building& building);
    static json::object OfficeToJson(const model::Office& office);
    static json::object MapToJson(const model::Map& map);

    const model::Game& game_;
    StaticFileHandler static_handler_;
};

template <class SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(const SomeRequestHandler& decorated)
        : decorated_(decorated) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const tcp::endpoint& remote_endpoint,
                    Send&& send) const {
        using namespace std::chrono;

        const auto start_time = steady_clock::now();
        const std::string ip = remote_endpoint.address().to_string();

        server_logger::LogEvent("request received", server_logger::MakeRequestData(req, ip));

        auto logging_send = [start_time, send = std::forward<Send>(send)](auto&& response) mutable {
            const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start_time).count();
            server_logger::LogEvent("response sent",
                                    server_logger::MakeResponseData(response, elapsed));
            send(std::forward<decltype(response)>(response));
        };

        decorated_(std::move(req), remote_endpoint, std::move(logging_send));
    }

private:
    const SomeRequestHandler& decorated_;
};

}  // namespace http_handler