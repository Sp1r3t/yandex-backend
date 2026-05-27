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
    if (value.is_double()) {
        return value.as_double();
    }
    if (value.is_int64()) {
        return static_cast<double>(value.as_int64());
    }
    if (value.is_uint64()) {
        return static_cast<double>(value.as_uint64());
    }
    throw std::invalid_argument("Expected number");
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

model::Map ParseMap(const json::object& obj, double default_dog_speed, unsigned default_bag_capacity) {
    const double dog_speed = obj.if_contains("dogSpeed") ? ParseDouble(obj.at("dogSpeed")) : default_dog_speed;

    unsigned bag_capacity = default_bag_capacity;
    if (const auto* bc = obj.if_contains("bagCapacity")) {
        if (bc->is_int64()) {
            bag_capacity = static_cast<unsigned>(bc->as_int64());
        } else if (bc->is_uint64()) {
            bag_capacity = static_cast<unsigned>(bc->as_uint64());
        }
    }

    model::Map map(model::Map::Id(std::string(obj.at("id").as_string().c_str())),
                   std::string(obj.at("name").as_string().c_str()),
                   dog_speed,
                   bag_capacity);

    if (const auto* loot_types = obj.if_contains("lootTypes")) {
        const auto& lt_array = loot_types->as_array();
        map.SetLootTypesCount(static_cast<unsigned>(lt_array.size()));

        std::vector<unsigned> values;
        values.reserve(lt_array.size());
        for (const auto& lt : lt_array) {
            const auto& lt_obj = lt.as_object();
            unsigned value = 0;
            if (const auto* v = lt_obj.if_contains("value")) {
                if (v->is_int64()) {
                    value = static_cast<unsigned>(v->as_int64());
                } else if (v->is_uint64()) {
                    value = static_cast<unsigned>(v->as_uint64());
                }
            }
            values.push_back(value);
        }
        map.SetLootTypeValues(std::move(values));
    }

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

LoadResult LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input(json_path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open config file: "s + json_path.string());
    }

    std::ostringstream strm;
    strm << input.rdbuf();

    json::value doc = json::parse(strm.str());
    const json::object& root = doc.as_object();

    const double default_dog_speed = root.if_contains("defaultDogSpeed")
        ? ParseDouble(root.at("defaultDogSpeed"))
        : 1.0;

    unsigned default_bag_capacity = model::Map::kDefaultBagCapacity;
    if (const auto* bc = root.if_contains("defaultBagCapacity")) {
        if (bc->is_int64()) {
            default_bag_capacity = static_cast<unsigned>(bc->as_int64());
        } else if (bc->is_uint64()) {
            default_bag_capacity = static_cast<unsigned>(bc->as_uint64());
        }
    }

    model::Game game;

    if (const auto* loot_cfg = root.if_contains("lootGeneratorConfig")) {
        const auto& cfg = loot_cfg->as_object();
        const double period_secs = ParseDouble(cfg.at("period"));
        const double probability = ParseDouble(cfg.at("probability"));
        const auto period_ms = model::Game::TimeInterval(
            static_cast<long long>(period_secs * 1000.0));
        game.SetLootGeneratorConfig(period_ms, probability);
    }

    extra_data::ExtraData extra_data;

    for (const auto& map_value : root.at("maps").as_array()) {
        const auto& obj = map_value.as_object();

        const std::string map_id_str(obj.at("id").as_string().c_str());

        if (const auto* loot_types = obj.if_contains("lootTypes")) {
            extra_data.AddMapLootTypes(map_id_str, loot_types->as_array());
        }

        game.AddMap(ParseMap(obj, default_dog_speed, default_bag_capacity));
    }

    return {std::move(game), std::move(extra_data)};
}

}  // namespace json_loader
