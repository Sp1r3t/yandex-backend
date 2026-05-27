#pragma once
#include <boost/json.hpp>
#include <string>
#include <unordered_map>

namespace extra_data {

namespace json = boost::json;

class ExtraData {
public:
    void AddMapLootTypes(const std::string& map_id, json::array loot_types) {
        loot_types_[map_id] = std::move(loot_types);
    }

    const json::array* GetMapLootTypes(const std::string& map_id) const {
        auto it = loot_types_.find(map_id);
        if (it == loot_types_.end()) {
            return nullptr;
        }
        return &it->second;
    }

private:
    std::unordered_map<std::string, json::array> loot_types_;
};

}  // namespace extra_data
