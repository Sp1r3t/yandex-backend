#include "request_handler.h"

namespace http_handler {

json::object RequestHandler::RoadToJson(const model::Road& road) {
    json::object obj;
    const auto start = road.GetStart();
    obj["x0"] = start.x;
    obj["y0"] = start.y;
    if (road.IsHorizontal()) {
        obj["x1"] = road.GetEnd().x;
    } else {
        obj["y1"] = road.GetEnd().y;
    }
    return obj;
}

json::object RequestHandler::BuildingToJson(const model::Building& building) {
    json::object obj;
    const auto& bounds = building.GetBounds();
    obj["x"] = bounds.position.x;
    obj["y"] = bounds.position.y;
    obj["w"] = bounds.size.width;
    obj["h"] = bounds.size.height;
    return obj;
}

json::object RequestHandler::OfficeToJson(const model::Office& office) {
    json::object obj;
    obj["id"] = *office.GetId();
    obj["x"] = office.GetPosition().x;
    obj["y"] = office.GetPosition().y;
    obj["offsetX"] = office.GetOffset().dx;
    obj["offsetY"] = office.GetOffset().dy;
    return obj;
}

json::object RequestHandler::MapToJson(const model::Map& map) {
    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();

    json::array roads;
    for (const auto& road : map.GetRoads()) {
        roads.emplace_back(RoadToJson(road));
    }
    obj["roads"] = std::move(roads);

    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        buildings.emplace_back(BuildingToJson(building));
    }
    obj["buildings"] = std::move(buildings);

    json::array offices;
    for (const auto& office : map.GetOffices()) {
        offices.emplace_back(OfficeToJson(office));
    }
    obj["offices"] = std::move(offices);

    return obj;
}

}  // namespace http_handler
