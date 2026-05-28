#pragma once

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "model.h"

// Non-intrusive Boost.Serialization for LostObject
namespace boost {
namespace serialization {

template <typename Archive>
void serialize(Archive& ar, model::LostObject& obj, [[maybe_unused]] unsigned int version) {
    ar & obj.id;
    ar & obj.type;
    ar & obj.pos.x;
    ar & obj.pos.y;
}

}  // namespace serialization
}  // namespace boost

namespace serialization {

namespace {

inline int DirectionToInt(model::Direction d) noexcept {
    switch (d) {
        case model::Direction::NORTH: return 0;
        case model::Direction::SOUTH: return 1;
        case model::Direction::WEST:  return 2;
        case model::Direction::EAST:  return 3;
    }
    return 0;
}

inline model::Direction IntToDirection(int d) noexcept {
    switch (d) {
        case 0: return model::Direction::NORTH;
        case 1: return model::Direction::SOUTH;
        case 2: return model::Direction::WEST;
        case 3: return model::Direction::EAST;
        default: return model::Direction::NORTH;
    }
}

struct PlayerRepr {
    model::Player::Id id = 0;
    std::string token;
    std::string map_id;
    std::string name;
    double pos_x = 0.0, pos_y = 0.0;
    double speed_vx = 0.0, speed_vy = 0.0;
    int direction = 0;
    std::vector<model::LostObject> bag;
    unsigned score = 0;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] unsigned int version) {
        ar & id;
        ar & token;
        ar & map_id;
        ar & name;
        ar & pos_x & pos_y;
        ar & speed_vx & speed_vy;
        ar & direction;
        ar & bag;
        ar & score;
    }
};

struct MapLostObjectsRepr {
    std::string map_id;
    std::vector<model::LostObject> objects;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] unsigned int version) {
        ar & map_id;
        ar & objects;
    }
};

struct GameStateRepr {
    std::vector<PlayerRepr> players;
    model::Player::Id next_player_id = 0;
    std::vector<MapLostObjectsRepr> map_lost_objects;
    unsigned next_lost_object_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] unsigned int version) {
        ar & players;
        ar & next_player_id;
        ar & map_lost_objects;
        ar & next_lost_object_id;
    }
};

}  // namespace

inline void SaveGame(const model::Game& game, const std::filesystem::path& path) {
    const auto players_data = game.GetPlayersForSerialization();
    const auto lost_objects_data = game.GetLostObjectsForSerialization();

    GameStateRepr state;
    state.next_player_id = game.GetNextPlayerId();
    state.next_lost_object_id = game.GetNextLostObjectId();

    state.players.reserve(players_data.size());
    for (const auto& pr : players_data) {
        PlayerRepr repr;
        repr.id = pr.id;
        repr.token = pr.token;
        repr.map_id = pr.map_id;
        repr.name = pr.name;
        repr.pos_x = pr.position.x;
        repr.pos_y = pr.position.y;
        repr.speed_vx = pr.speed.vx;
        repr.speed_vy = pr.speed.vy;
        repr.direction = DirectionToInt(pr.direction);
        repr.bag = pr.bag;
        repr.score = pr.score;
        state.players.push_back(std::move(repr));
    }

    state.map_lost_objects.reserve(lost_objects_data.size());
    for (const auto& [map_id, objects] : lost_objects_data) {
        MapLostObjectsRepr mr;
        mr.map_id = map_id;
        mr.objects = objects;
        state.map_lost_objects.push_back(std::move(mr));
    }

    // Write atomically: write to temp file then rename
    auto tmp_path = path;
    tmp_path += ".tmp";

    {
        std::ofstream ofs(tmp_path);
        if (!ofs) {
            throw std::runtime_error("Failed to open temp state file for writing: " + tmp_path.string());
        }
        boost::archive::text_oarchive oa(ofs);
        oa << state;
    }

    std::filesystem::rename(tmp_path, path);
}

inline void LoadGame(model::Game& game, const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Failed to open state file: " + path.string());
    }

    GameStateRepr state;
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> state;
    }

    std::vector<model::Game::PlayerRecord> players;
    players.reserve(state.players.size());
    for (const auto& repr : state.players) {
        model::Game::PlayerRecord pr;
        pr.id = repr.id;
        pr.token = repr.token;
        pr.map_id = repr.map_id;
        pr.name = repr.name;
        pr.position = {repr.pos_x, repr.pos_y};
        pr.speed = {repr.speed_vx, repr.speed_vy};
        pr.direction = IntToDirection(repr.direction);
        pr.bag = repr.bag;
        pr.score = repr.score;
        players.push_back(std::move(pr));
    }

    std::vector<std::pair<std::string, std::vector<model::LostObject>>> lost_objects;
    lost_objects.reserve(state.map_lost_objects.size());
    for (const auto& mr : state.map_lost_objects) {
        lost_objects.emplace_back(mr.map_id, mr.objects);
    }

    game.RestoreState(std::move(players), state.next_player_id,
                      std::move(lost_objects), state.next_lost_object_id);
}

}  // namespace serialization
