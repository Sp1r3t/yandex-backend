#include "request_handler.h"

#include <algorithm>
#include <iterator>

namespace http_handler {

namespace {

constexpr json::string_view kFieldX0      = "x0";
constexpr json::string_view kFieldY0      = "y0";
constexpr json::string_view kFieldX1      = "x1";
constexpr json::string_view kFieldY1      = "y1";
constexpr json::string_view kFieldX       = "x";
constexpr json::string_view kFieldY       = "y";
constexpr json::string_view kFieldW       = "w";
constexpr json::string_view kFieldH       = "h";
constexpr json::string_view kFieldId      = "id";
constexpr json::string_view kFieldName    = "name";
constexpr json::string_view kFieldOffsetX = "offsetX";
constexpr json::string_view kFieldOffsetY = "offsetY";
constexpr json::string_view kFieldRoads     = "roads";
constexpr json::string_view kFieldBuildings = "buildings";
constexpr json::string_view kFieldOffices   = "offices";

}  // namespace

json::object RequestHandler::RoadToJson(const model::Road& road) {
    json::object obj;
    const auto start = road.GetStart();
    obj[kFieldX0] = start.x;
    obj[kFieldY0] = start.y;
    if (road.IsHorizontal()) {
        obj[kFieldX1] = road.GetEnd().x;
    } else {
        obj[kFieldY1] = road.GetEnd().y;
    }
    return obj;
}

json::object RequestHandler::BuildingToJson(const model::Building& building) {
    json::object obj;
    const auto& bounds = building.GetBounds();
    obj[kFieldX] = bounds.position.x;
    obj[kFieldY] = bounds.position.y;
    obj[kFieldW] = bounds.size.width;
    obj[kFieldH] = bounds.size.height;
    return obj;
}

json::object RequestHandler::OfficeToJson(const model::Office& office) {
    json::object obj;
    obj[kFieldId]      = *office.GetId();
    obj[kFieldX]       = office.GetPosition().x;
    obj[kFieldY]       = office.GetPosition().y;
    obj[kFieldOffsetX] = office.GetOffset().dx;
    obj[kFieldOffsetY] = office.GetOffset().dy;
    return obj;
}

json::object RequestHandler::MapToJson(const model::Map& map) {
    json::object obj;
    obj[kFieldId]   = *map.GetId();
    obj[kFieldName] = map.GetName();

    const auto to_json_value = [](auto convert_fn) {
        return [convert_fn](const auto& item) -> json::value {
            return convert_fn(item);
        };
    };

    json::array roads;
    std::transform(map.GetRoads().begin(), map.GetRoads().end(),
                   std::back_inserter(roads),
                   to_json_value(RoadToJson));
    obj[kFieldRoads] = std::move(roads);

    json::array buildings;
    std::transform(map.GetBuildings().begin(), map.GetBuildings().end(),
                   std::back_inserter(buildings),
                   to_json_value(BuildingToJson));
    obj[kFieldBuildings] = std::move(buildings);

    json::array offices;
    std::transform(map.GetOffices().begin(), map.GetOffices().end(),
                   std::back_inserter(offices),
                   to_json_value(OfficeToJson));
    obj[kFieldOffices] = std::move(offices);

    return obj;
}

}  // namespace http_handler
