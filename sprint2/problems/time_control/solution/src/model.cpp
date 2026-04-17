#include "model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace model {
using namespace std::literals;

namespace {

constexpr double kRoadHalfWidth = 0.4;
constexpr double kEpsilon = 1e-9;

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

PointD GetSpawnPointOnMap(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return PointD{0.0, 0.0};
    }

    const Point start = roads.front().GetStart();
    return PointD{static_cast<double>(start.x), static_cast<double>(start.y)};
}

struct Interval {
    double min = 0.0;
    double max = 0.0;
};

std::vector<Interval> BuildHorizontalIntervals(const Map& map, double y) {
    std::vector<Interval> intervals;
    for (const auto& road : map.GetRoads()) {
        if (!road.IsHorizontal()) {
            continue;
        }

        const auto start = road.GetStart();
        const auto end = road.GetEnd();
        if (std::abs(y - static_cast<double>(start.y)) > kRoadHalfWidth + kEpsilon) {
            continue;
        }

        const double min_x = static_cast<double>(std::min(start.x, end.x)) - kRoadHalfWidth;
        const double max_x = static_cast<double>(std::max(start.x, end.x)) + kRoadHalfWidth;
        intervals.push_back({min_x, max_x});
    }

    std::sort(intervals.begin(), intervals.end(), [](const Interval& lhs, const Interval& rhs) {
        if (lhs.min != rhs.min) {
            return lhs.min < rhs.min;
        }
        return lhs.max < rhs.max;
    });

    std::vector<Interval> merged;
    for (const auto& interval : intervals) {
        if (merged.empty() || interval.min > merged.back().max + kEpsilon) {
            merged.push_back(interval);
        } else {
            merged.back().max = std::max(merged.back().max, interval.max);
        }
    }
    return merged;
}

std::vector<Interval> BuildVerticalIntervals(const Map& map, double x) {
    std::vector<Interval> intervals;
    for (const auto& road : map.GetRoads()) {
        if (!road.IsVertical()) {
            continue;
        }

        const auto start = road.GetStart();
        const auto end = road.GetEnd();
        if (std::abs(x - static_cast<double>(start.x)) > kRoadHalfWidth + kEpsilon) {
            continue;
        }

        const double min_y = static_cast<double>(std::min(start.y, end.y)) - kRoadHalfWidth;
        const double max_y = static_cast<double>(std::max(start.y, end.y)) + kRoadHalfWidth;
        intervals.push_back({min_y, max_y});
    }

    std::sort(intervals.begin(), intervals.end(), [](const Interval& lhs, const Interval& rhs) {
        if (lhs.min != rhs.min) {
            return lhs.min < rhs.min;
        }
        return lhs.max < rhs.max;
    });

    std::vector<Interval> merged;
    for (const auto& interval : intervals) {
        if (merged.empty() || interval.min > merged.back().max + kEpsilon) {
            merged.push_back(interval);
        } else {
            merged.back().max = std::max(merged.back().max, interval.max);
        }
    }
    return merged;
}

const Interval* FindContainingInterval(const std::vector<Interval>& intervals, double value) {
    for (const auto& interval : intervals) {
        if (value >= interval.min - kEpsilon && value <= interval.max + kEpsilon) {
            return &interval;
        }
    }
    return nullptr;
}

void MoveDogOnMap(const Map& map, Dog& dog, std::int64_t time_delta_ms) {
    if (time_delta_ms <= 0) {
        return;
    }

    const Speed2D speed = dog.GetSpeed();
    if (std::abs(speed.vx) < kEpsilon && std::abs(speed.vy) < kEpsilon) {
        return;
    }

    const double dt = static_cast<double>(time_delta_ms) / 1000.0;
    PointD position = dog.GetPosition();

    if (std::abs(speed.vx) > kEpsilon) {
        const auto intervals = BuildHorizontalIntervals(map, position.y);
        const Interval* interval = FindContainingInterval(intervals, position.x);
        if (!interval) {
            dog.SetSpeed({0.0, 0.0});
            return;
        }

        const double target_x = position.x + speed.vx * dt;
        const double clamped_x = std::clamp(target_x, interval->min, interval->max);
        dog.SetPosition({clamped_x, position.y});
        if (std::abs(clamped_x - target_x) > kEpsilon) {
            dog.SetSpeed({0.0, 0.0});
        }
        return;
    }

    const auto intervals = BuildVerticalIntervals(map, position.x);
    const Interval* interval = FindContainingInterval(intervals, position.y);
    if (!interval) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

    const double target_y = position.y + speed.vy * dt;
    const double clamped_y = std::clamp(target_y, interval->min, interval->max);
    dog.SetPosition({position.x, clamped_y});
    if (std::abs(clamped_y - target_y) > kEpsilon) {
        dog.SetSpeed({0.0, 0.0});
    }
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

    const PointD spawn_position = GetSpawnPointOnMap(*map);

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

bool Game::SetPlayerAction(std::string_view token, MoveCommand move) const {
    std::lock_guard guard(players_mutex_);

    const auto it = token_to_player_index_.find(std::string(token));
    if (it == token_to_player_index_.end()) {
        return false;
    }

    Player& player = players_.at(it->second);
    Dog& dog = player.GetDog();

    const Map* map = FindMap(player.GetMapId());
    const double speed = map ? map->GetDogSpeed() : 1.0;

    switch (move) {
        case MoveCommand::LEFT:
            dog.SetDirection(Direction::WEST);
            dog.SetSpeed({-speed, 0.0});
            break;
        case MoveCommand::RIGHT:
            dog.SetDirection(Direction::EAST);
            dog.SetSpeed({speed, 0.0});
            break;
        case MoveCommand::UP:
            dog.SetDirection(Direction::NORTH);
            dog.SetSpeed({0.0, -speed});
            break;
        case MoveCommand::DOWN:
            dog.SetDirection(Direction::SOUTH);
            dog.SetSpeed({0.0, speed});
            break;
        case MoveCommand::STOP:
            dog.SetSpeed({0.0, 0.0});
            break;
    }

    return true;
}

void Game::Tick(std::int64_t time_delta_ms) const {
    std::lock_guard guard(players_mutex_);

    for (Player& player : players_) {
        if (const Map* map = FindMap(player.GetMapId())) {
            MoveDogOnMap(*map, player.GetDog(), time_delta_ms);
        }
    }
}

}  // namespace model
