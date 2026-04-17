#include "json_loader.h"

#include <boost/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace json_loader {
namespace json = boost::json;
using namespace std::literals;

namespace {

double ParseDouble(const json::value& value) {
    if (const auto* double_value = value.if_double()) {
        return *double_value;
    }
    if (const auto* int_value = value.if_int64()) {
        return static_cast<double>(*int_value);
    }
    if (const auto* uint_value = value.if_uint64()) {
        return static_cast<double>(*uint_value);
    }
    throw std::runtime_error("Numeric value is expected");
}

model::Road ParseRoad(const json::object& obj) {
    const model::Point start{
        static_cast<model::Coord>(obj.at("x0").as_int64()),
        static_cast<model::Coord>(obj.at("y0").as_int64())};

    if (obj.if_contains("x1")) {
        return model::Road(model::Road::HORIZONTAL, start,
                           static_cast<model::Coord>(obj.at("x1").as_int64()));
    }
    return model::Road(model::Road::VERTICAL, start,
                       static_cast<model::Coord>(obj.at("y1").as_int64()));
}

model::Building ParseBuilding(const json::object& obj) {
    return model::Building({
        {static_cast<model::Coord>(obj.at("x").as_int64()), static_cast<model::Coord>(obj.at("y").as_int64())},
        {static_cast<model::Dimension>(obj.at("w").as_int64()), static_cast<model::Dimension>(obj.at("h").as_int64())}
    });
}

model::Office ParseOffice(const json::object& obj) {
    return model::Office(
        model::Office::Id(std::string(obj.at("id").as_string().c_str())),
        {static_cast<model::Coord>(obj.at("x").as_int64()), static_cast<model::Coord>(obj.at("y").as_int64())},
        {static_cast<model::Dimension>(obj.at("offsetX").as_int64()), static_cast<model::Dimension>(obj.at("offsetY").as_int64())});
}

model::Map ParseMap(const json::object& obj, double default_dog_speed) {
    double dog_speed = default_dog_speed;
    if (const auto* dog_speed_value = obj.if_contains("dogSpeed")) {
        dog_speed = ParseDouble(*dog_speed_value);
    }

    model::Map map(model::Map::Id(std::string(obj.at("id").as_string().c_str())),
                   std::string(obj.at("name").as_string().c_str()),
                   dog_speed);

    for (const auto& road : obj.at("roads").as_array()) {
        map.AddRoad(ParseRoad(road.as_object()));
    }
    for (const auto& building : obj.at("buildings").as_array()) {
        map.AddBuilding(ParseBuilding(building.as_object()));
    }
    for (const auto& office : obj.at("offices").as_array()) {
        map.AddOffice(ParseOffice(office.as_object()));
    }
    return map;
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input(json_path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open config file: "s + json_path.string());
    }

    std::ostringstream strm;
    strm << input.rdbuf();

    json::value doc = json::parse(strm.str());
    const json::object& root = doc.as_object();

    double default_dog_speed = 1.0;
    if (const auto* default_speed_value = root.if_contains("defaultDogSpeed")) {
        default_dog_speed = ParseDouble(*default_speed_value);
    }

    model::Game game;
    for (const auto& map_value : root.at("maps").as_array()) {
        game.AddMap(ParseMap(map_value.as_object(), default_dog_speed));
    }
    return game;
}

}  // namespace json_loader
