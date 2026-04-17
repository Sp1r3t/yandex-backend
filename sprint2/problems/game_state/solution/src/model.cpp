#include "model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace {

std::mt19937_64& GetRandomEngine() {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

std::string GenerateToken() {
    static constexpr std::array<char, 16> kHexDigits = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    std::uniform_int_distribution<int> distribution(0, static_cast<int>(kHexDigits.size() - 1));

    std::string token(32, '0');
    for (char& ch : token) {
        ch = kHexDigits[distribution(GetRandomEngine())];
    }
    return token;
}

PointD PickRandomPointOnRoads(const Map::Roads& roads) {
    if (roads.empty()) {
        return PointD{0.0, 0.0};
    }

    auto& engine = GetRandomEngine();
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    const Road& road = roads[road_dist(engine)];

    const auto start = road.GetStart();
    const auto end = road.GetEnd();

    std::uniform_real_distribution<double> t_dist(0.0, 1.0);
    const double t = t_dist(engine);

    const double x = static_cast<double>(start.x) + t * (static_cast<double>(end.x) - static_cast<double>(start.x));
    const double y = static_cast<double>(start.y) + t * (static_cast<double>(end.y) - static_cast<double>(start.y));

    return PointD{x, y};
}

}  // namespace

Game::Game(Game&& other) noexcept {
    std::lock_guard guard(other.players_mutex_);
    maps_ = std::move(other.maps_);
    map_id_to_index_ = std::move(other.map_id_to_index_);
    players_ = std::move(other.players_);
    token_to_player_index_ = std::move(other.token_to_player_index_);
    next_player_id_ = other.next_player_id_;
}

Game& Game::operator=(Game&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(players_mutex_, other.players_mutex_);
        maps_ = std::move(other.maps_);
        map_id_to_index_ = std::move(other.map_id_to_index_);
        players_ = std::move(other.players_);
        token_to_player_index_ = std::move(other.token_to_player_index_);
        next_player_id_ = other.next_player_id_;
    }
    return *this;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.find(office.GetId()) != warehouse_id_to_index_.end()) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    warehouse_id_to_index_.emplace(o.GetId(), index);
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::optional<Game::JoinGameResult> Game::JoinPlayer(const Map::Id& map_id, std::string user_name) const {
    const Map* map = FindMap(map_id);
    if (!map) {
        return std::nullopt;
    }

    const PointD spawn_position = PickRandomPointOnRoads(map->GetRoads());

    std::lock_guard guard(players_mutex_);

    std::string token;
    do {
        token = GenerateToken();
    } while (token_to_player_index_.count(token) != 0);

    const auto player_id = next_player_id_++;
    const auto player_index = players_.size();

    players_.emplace_back(player_id, map_id, Dog(std::move(user_name), spawn_position));
    token_to_player_index_.emplace(token, player_index);

    return JoinGameResult{player_id, std::move(token)};
}

std::optional<Game::PlayerInfo> Game::FindPlayerByToken(std::string_view token) const {
    std::lock_guard guard(players_mutex_);

    const auto it = token_to_player_index_.find(std::string(token));
    if (it == token_to_player_index_.end()) {
        return std::nullopt;
    }

    const Player& player = players_.at(it->second);
    return PlayerInfo{player.GetId(), player.GetMapId(), player.GetDog().GetName()};
}

std::vector<Game::PlayerInfo> Game::GetPlayersOnMap(const Map::Id& map_id) const {
    std::lock_guard guard(players_mutex_);

    std::vector<PlayerInfo> result;
    result.reserve(players_.size());

    for (const Player& player : players_) {
        if (player.GetMapId() == map_id) {
            result.push_back(PlayerInfo{player.GetId(), player.GetMapId(), player.GetDog().GetName()});
        }
    }

    return result;
}

std::optional<std::vector<Game::PlayerSnapshot>> Game::GetStateByToken(std::string_view token) const {
    std::lock_guard guard(players_mutex_);

    const auto it = token_to_player_index_.find(std::string(token));
    if (it == token_to_player_index_.end()) {
        return std::nullopt;
    }

    const Map::Id map_id = players_.at(it->second).GetMapId();

    std::vector<PlayerSnapshot> result;
    result.reserve(players_.size());
    for (const Player& player : players_) {
        if (player.GetMapId() == map_id) {
            const Dog& dog = player.GetDog();
            result.push_back(PlayerSnapshot{
                player.GetId(),
                dog.GetPosition(),
                dog.GetSpeed(),
                dog.GetDirection()
            });
        }
    }

    return result;
}

}  // namespace model
