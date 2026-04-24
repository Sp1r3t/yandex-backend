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

PointD GetDefaultSpawnPointOnMap(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return PointD{0.0, 0.0};
    }

    const Point start = roads.front().GetStart();
    return PointD{static_cast<double>(start.x), static_cast<double>(start.y)};
}

PointD GetRandomSpawnPointOnMap(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return PointD{0.0, 0.0};
    }

    std::uniform_int_distribution<size_t> road_distribution(0, roads.size() - 1);
    const Road& road = roads[road_distribution(GetRandomEngine())];

    const Point start = road.GetStart();
    const Point end = road.GetEnd();

    if (road.IsHorizontal()) {
        const int min_x = std::min(start.x, end.x);
        const int max_x = std::max(start.x, end.x);
        std::uniform_real_distribution<double> distribution(static_cast<double>(min_x), static_cast<double>(max_x));
        return PointD{distribution(GetRandomEngine()), static_cast<double>(start.y)};
    }

    const int min_y = std::min(start.y, end.y);
    const int max_y = std::max(start.y, end.y);
    std::uniform_real_distribution<double> distribution(static_cast<double>(min_y), static_cast<double>(max_y));
    return PointD{static_cast<double>(start.x), distribution(GetRandomEngine())};
}

PointD GetSpawnPointOnMap(const Map& map, bool randomize_spawn_points) {
    if (randomize_spawn_points) {
        return GetRandomSpawnPointOnMap(map);
    }
    return GetDefaultSpawnPointOnMap(map);
}

struct Interval {
    double min = 0.0;
    double max = 0.0;
};

// Общая функция: собирает проекции всех дорог по заданной оси и сливает пересекающиеся отрезки.
// AddProjectionFn должна иметь сигнатуру: void(const Road&, std::vector<Interval>&)
template <typename AddProjectionFn>
std::vector<Interval> BuildMergedIntervals(const Map& map, AddProjectionFn add_projection) {
    std::vector<Interval> intervals;
    for (const auto& road : map.GetRoads()) {
        add_projection(road, intervals);
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

void AddRoadProjectionByY(const Road& road, double y, std::vector<Interval>& intervals) {
    const auto start = road.GetStart();
    const auto end = road.GetEnd();

    if (road.IsHorizontal()) {
        const double center_y = static_cast<double>(start.y);
        if (std::abs(y - center_y) > kRoadHalfWidth + kEpsilon) {
            return;
        }
        intervals.push_back({
            static_cast<double>(std::min(start.x, end.x)) - kRoadHalfWidth,
            static_cast<double>(std::max(start.x, end.x)) + kRoadHalfWidth});
    } else {
        const double min_y = static_cast<double>(std::min(start.y, end.y)) - kRoadHalfWidth;
        const double max_y = static_cast<double>(std::max(start.y, end.y)) + kRoadHalfWidth;
        if (y < min_y - kEpsilon || y > max_y + kEpsilon) {
            return;
        }
        const double center_x = static_cast<double>(start.x);
        intervals.push_back({center_x - kRoadHalfWidth, center_x + kRoadHalfWidth});
    }
}

void AddRoadProjectionByX(const Road& road, double x, std::vector<Interval>& intervals) {
    const auto start = road.GetStart();
    const auto end = road.GetEnd();

    if (road.IsVertical()) {
        const double center_x = static_cast<double>(start.x);
        if (std::abs(x - center_x) > kRoadHalfWidth + kEpsilon) {
            return;
        }
        intervals.push_back({
            static_cast<double>(std::min(start.y, end.y)) - kRoadHalfWidth,
            static_cast<double>(std::max(start.y, end.y)) + kRoadHalfWidth});
    } else {
        const double min_x = static_cast<double>(std::min(start.x, end.x)) - kRoadHalfWidth;
        const double max_x = static_cast<double>(std::max(start.x, end.x)) + kRoadHalfWidth;
        if (x < min_x - kEpsilon || x > max_x + kEpsilon) {
            return;
        }
        const double center_y = static_cast<double>(start.y);
        intervals.push_back({center_y - kRoadHalfWidth, center_y + kRoadHalfWidth});
    }
}

std::vector<Interval> BuildHorizontalIntervals(const Map& map, double y) {
    return BuildMergedIntervals(map, [y](const Road& road, std::vector<Interval>& intervals) {
        AddRoadProjectionByY(road, y, intervals);
    });
}

std::vector<Interval> BuildVerticalIntervals(const Map& map, double x) {
    return BuildMergedIntervals(map, [x](const Road& road, std::vector<Interval>& intervals) {
        AddRoadProjectionByX(road, x, intervals);
    });
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
    const Speed2D speed = dog.GetSpeed();
    if (std::abs(speed.vx) <= kEpsilon && std::abs(speed.vy) <= kEpsilon) {
        return;
    }

    const double dt = static_cast<double>(time_delta_ms) / 1000.0;
    PointD position = dog.GetPosition();

    if (std::abs(speed.vx) > kEpsilon) {
        const auto intervals = BuildHorizontalIntervals(map, position.y);
        const Interval* current = FindContainingInterval(intervals, position.x);
        if (!current) {
            dog.SetSpeed({0.0, 0.0});
            return;
        }

        const double target_x = position.x + speed.vx * dt;
        if (speed.vx > 0.0) {
            if (target_x <= current->max + kEpsilon) {
                position.x = target_x;
            } else {
                position.x = current->max;
                dog.SetSpeed({0.0, 0.0});
            }
        } else {
            if (target_x >= current->min - kEpsilon) {
                position.x = target_x;
            } else {
                position.x = current->min;
                dog.SetSpeed({0.0, 0.0});
            }
        }
        dog.SetPosition(position);
        return;
    }

    const auto intervals = BuildVerticalIntervals(map, position.x);
    const Interval* current = FindContainingInterval(intervals, position.y);
    if (!current) {
        dog.SetSpeed({0.0, 0.0});
        return;
    }

    const double target_y = position.y + speed.vy * dt;
    if (speed.vy > 0.0) {
        if (target_y <= current->max + kEpsilon) {
            position.y = target_y;
        } else {
            position.y = current->max;
            dog.SetSpeed({0.0, 0.0});
        }
    } else {
        if (target_y >= current->min - kEpsilon) {
            position.y = target_y;
        } else {
            position.y = current->min;
            dog.SetSpeed({0.0, 0.0});
        }
    }
    dog.SetPosition(position);
}

}  // namespace

Game::Game(Game&& other) noexcept {
    std::lock_guard guard(other.players_mutex_);
    maps_ = std::move(other.maps_);
    map_id_to_index_ = std::move(other.map_id_to_index_);
    randomize_spawn_points_ = other.randomize_spawn_points_;
    players_ = std::move(other.players_);
    token_to_player_index_ = std::move(other.token_to_player_index_);
    next_player_id_ = other.next_player_id_;
}

Game& Game::operator=(Game&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(players_mutex_, other.players_mutex_);
        maps_ = std::move(other.maps_);
        map_id_to_index_ = std::move(other.map_id_to_index_);
        randomize_spawn_points_ = other.randomize_spawn_points_;
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

    const PointD spawn_position = GetSpawnPointOnMap(*map, randomize_spawn_points_);

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
                dog.GetDirection()});
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
