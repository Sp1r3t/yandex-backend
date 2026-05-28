#include "model.h"
#include "collision_detector.h"

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

constexpr double kDogHalfWidth    = 0.3;
constexpr double kItemHalfWidth   = 0.0;
constexpr double kOfficeHalfWidth = 0.25;

std::mt19937_64& GetRandomEngine() {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

double GameRandomGen() noexcept {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(GetRandomEngine());
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

unsigned GenerateRandomLootType(unsigned count) {
    if (count == 0) return 0;
    std::uniform_int_distribution<unsigned> dist(0, count - 1);
    return dist(GetRandomEngine());
}

PointD GenerateRandomPositionOnRoad(const Road& road) {
    const auto start = road.GetStart();
    const auto end = road.GetEnd();

    if (road.IsHorizontal()) {
        const double min_x = static_cast<double>(std::min(start.x, end.x));
        const double max_x = static_cast<double>(std::max(start.x, end.x));
        std::uniform_real_distribution<double> x_dist(min_x, max_x);
        return {x_dist(GetRandomEngine()), static_cast<double>(start.y)};
    } else {
        const double min_y = static_cast<double>(std::min(start.y, end.y));
        const double max_y = static_cast<double>(std::max(start.y, end.y));
        std::uniform_real_distribution<double> y_dist(min_y, max_y);
        return {static_cast<double>(start.x), y_dist(GetRandomEngine())};
    }
}

PointD GenerateRandomPositionOnMap(const Map& map) {
    const auto& roads = map.GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
    return GenerateRandomPositionOnRoad(roads[road_dist(GetRandomEngine())]);
}

struct Interval {
    double min = 0.0;
    double max = 0.0;
};

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
    std::vector<Interval> intervals;
    for (const auto& road : map.GetRoads()) {
        AddRoadProjectionByY(road, y, intervals);
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
        AddRoadProjectionByX(road, x, intervals);
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

struct MapGathererProvider : collision_detector::ItemGathererProvider {
    std::vector<collision_detector::Item> items;
    std::vector<collision_detector::Gatherer> gatherers;

    size_t ItemsCount() const override { return items.size(); }
    collision_detector::Item GetItem(size_t idx) const override { return items[idx]; }
    size_t GatherersCount() const override { return gatherers.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers[idx]; }
};

}  // namespace

Game::Game(Game&& other) noexcept {
    std::lock_guard guard(other.players_mutex_);
    maps_ = std::move(other.maps_);
    map_id_to_index_ = std::move(other.map_id_to_index_);
    loot_period_ = other.loot_period_;
    loot_probability_ = other.loot_probability_;
    loot_random_gen_ = std::move(other.loot_random_gen_);
    players_ = std::move(other.players_);
    token_to_player_index_ = std::move(other.token_to_player_index_);
    next_player_id_ = other.next_player_id_;
    map_generators_ = std::move(other.map_generators_);
    map_lost_objects_ = std::move(other.map_lost_objects_);
    next_lost_object_id_ = other.next_lost_object_id_;
}

Game& Game::operator=(Game&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(players_mutex_, other.players_mutex_);
        maps_ = std::move(other.maps_);
        map_id_to_index_ = std::move(other.map_id_to_index_);
        loot_period_ = other.loot_period_;
        loot_probability_ = other.loot_probability_;
        loot_random_gen_ = std::move(other.loot_random_gen_);
        players_ = std::move(other.players_);
        token_to_player_index_ = std::move(other.token_to_player_index_);
        next_player_id_ = other.next_player_id_;
        map_generators_ = std::move(other.map_generators_);
        map_lost_objects_ = std::move(other.map_lost_objects_);
        next_lost_object_id_ = other.next_lost_object_id_;
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

void Game::SetLootGeneratorConfig(TimeInterval period, double probability,
                                   loot_gen::LootGenerator::RandomGenerator random_gen) {
    loot_period_ = period;
    loot_probability_ = probability;
    loot_random_gen_ = std::move(random_gen);
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            const auto& added_map = maps_.emplace_back(std::move(map));
            const Map::Id& map_id = added_map.GetId();

            if (loot_random_gen_) {
                map_generators_.try_emplace(map_id, loot_period_, loot_probability_, loot_random_gen_);
            } else {
                map_generators_.try_emplace(map_id, loot_period_, loot_probability_);
            }
            map_lost_objects_.try_emplace(map_id);
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

std::optional<Game::GameState> Game::GetStateByToken(std::string_view token) const {
    std::lock_guard guard(players_mutex_);

    const auto it = token_to_player_index_.find(std::string(token));
    if (it == token_to_player_index_.end()) {
        return std::nullopt;
    }

    const Map::Id map_id = players_.at(it->second).GetMapId();

    GameState state;
    state.players.reserve(players_.size());
    for (const Player& player : players_) {
        if (player.GetMapId() == map_id) {
            const Dog& dog = player.GetDog();
            state.players.push_back(PlayerSnapshot{
                player.GetId(),
                dog.GetPosition(),
                dog.GetSpeed(),
                dog.GetDirection(),
                dog.GetBag(),
                dog.GetScore()});
        }
    }

    if (const auto loot_it = map_lost_objects_.find(map_id); loot_it != map_lost_objects_.end()) {
        state.lost_objects = loot_it->second;
    }

    return state;
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

    std::vector<PointD> start_positions(players_.size());
    for (size_t i = 0; i < players_.size(); ++i) {
        start_positions[i] = players_[i].GetDog().GetPosition();
    }

    for (Player& player : players_) {
        if (const Map* map = FindMap(player.GetMapId())) {
            MoveDogOnMap(*map, player.GetDog(), time_delta_ms);
        }
    }

    std::unordered_map<Map::Id, std::vector<size_t>, MapIdHasher> map_players;
    for (size_t i = 0; i < players_.size(); ++i) {
        map_players[players_[i].GetMapId()].push_back(i);
    }

    for (const auto& [map_id, player_indices] : map_players) {
        const Map* map = FindMap(map_id);
        if (!map) continue;

        auto& lost_objects = map_lost_objects_.at(map_id);
        const size_t loot_count = lost_objects.size();

        MapGathererProvider provider;

        for (const auto& obj : lost_objects) {
            provider.items.push_back({{obj.pos.x, obj.pos.y}, kItemHalfWidth});
        }

        for (const auto& office : map->GetOffices()) {
            provider.items.push_back({
                {static_cast<double>(office.GetPosition().x),
                 static_cast<double>(office.GetPosition().y)},
                kOfficeHalfWidth});
        }

        for (size_t pi : player_indices) {
            provider.gatherers.push_back({
                {start_positions[pi].x, start_positions[pi].y},
                {players_[pi].GetDog().GetPosition().x, players_[pi].GetDog().GetPosition().y},
                kDogHalfWidth});
        }

        const auto events = collision_detector::FindGatherEvents(provider);

        std::vector<bool> item_collected(loot_count, false);

        for (const auto& event : events) {
            const size_t pi = player_indices[event.gatherer_id];
            Dog& dog = players_[pi].GetDog();

            if (event.item_id < loot_count) {
                if (item_collected[event.item_id]) continue;
                if (dog.GetBagSize() >= map->GetBagCapacity()) continue;

                dog.AddToBag(lost_objects[event.item_id]);
                item_collected[event.item_id] = true;
            } else {
                for (const auto& item : dog.GetBag()) {
                    dog.AddScore(map->GetLootTypeValue(item.type));
                }
                dog.ClearBag();
            }
        }

        std::vector<LostObject> remaining;
        remaining.reserve(loot_count);
        for (size_t i = 0; i < loot_count; ++i) {
            if (!item_collected[i]) {
                remaining.push_back(lost_objects[i]);
            }
        }
        for (size_t i = loot_count; i < lost_objects.size(); ++i) {
            remaining.push_back(lost_objects[i]);
        }
        lost_objects = std::move(remaining);
    }

    std::unordered_map<Map::Id, unsigned, MapIdHasher> players_per_map;
    for (const auto& [map_id, indices] : map_players) {
        players_per_map[map_id] = static_cast<unsigned>(indices.size());
    }

    const auto time_delta = TimeInterval(time_delta_ms);

    for (auto& [map_id, generator] : map_generators_) {
        auto& lost_objects = map_lost_objects_.at(map_id);
        const unsigned loot_count = static_cast<unsigned>(lost_objects.size());

        unsigned looter_count = 0;
        if (auto pc_it = players_per_map.find(map_id); pc_it != players_per_map.end()) {
            looter_count = pc_it->second;
        }

        const unsigned new_count = generator.Generate(time_delta, loot_count, looter_count);

        if (new_count > 0) {
            const Map* map = FindMap(map_id);
            if (!map || map->GetRoads().empty()) {
                continue;
            }
            for (unsigned i = 0; i < new_count; ++i) {
                LostObject obj;
                obj.id = next_lost_object_id_++;
                obj.type = GenerateRandomLootType(map->GetLootTypesCount());
                obj.pos = GenerateRandomPositionOnMap(*map);
                lost_objects.push_back(std::move(obj));
            }
        }
    }
}

std::vector<Game::PlayerRecord> Game::GetPlayersForSerialization() const {
    std::lock_guard guard(players_mutex_);

    // Build reverse map: player index -> token
    std::unordered_map<size_t, std::string> index_to_token;
    index_to_token.reserve(token_to_player_index_.size());
    for (const auto& [token, index] : token_to_player_index_) {
        index_to_token[index] = token;
    }

    std::vector<PlayerRecord> result;
    result.reserve(players_.size());

    for (size_t i = 0; i < players_.size(); ++i) {
        const Player& player = players_[i];
        const Dog& dog = player.GetDog();

        PlayerRecord pr;
        pr.id = player.GetId();
        pr.token = index_to_token.count(i) ? index_to_token.at(i) : "";
        pr.map_id = *player.GetMapId();
        pr.name = dog.GetName();
        pr.position = dog.GetPosition();
        pr.speed = dog.GetSpeed();
        pr.direction = dog.GetDirection();
        pr.bag = dog.GetBag();
        pr.score = dog.GetScore();

        result.push_back(std::move(pr));
    }

    return result;
}

std::vector<std::pair<std::string, std::vector<LostObject>>>
Game::GetLostObjectsForSerialization() const {
    std::lock_guard guard(players_mutex_);

    std::vector<std::pair<std::string, std::vector<LostObject>>> result;
    result.reserve(map_lost_objects_.size());

    for (const auto& [map_id, objects] : map_lost_objects_) {
        result.emplace_back(*map_id, objects);
    }

    return result;
}

void Game::RestoreState(std::vector<PlayerRecord> players,
                        Player::Id next_player_id,
                        std::vector<std::pair<std::string, std::vector<LostObject>>> lost_objects,
                        unsigned next_lost_object_id) {
    std::lock_guard guard(players_mutex_);

    players_.clear();
    token_to_player_index_.clear();

    for (auto& pr : players) {
        const size_t index = players_.size();

        Dog dog(pr.name, pr.position);
        dog.SetSpeed(pr.speed);
        dog.SetDirection(pr.direction);
        for (auto& item : pr.bag) {
            dog.AddToBag(std::move(item));
        }
        dog.AddScore(pr.score);

        players_.emplace_back(pr.id, Map::Id(pr.map_id), std::move(dog));

        if (!pr.token.empty()) {
            token_to_player_index_[pr.token] = index;
        }
    }

    next_player_id_ = next_player_id;

    for (auto& [map_id_str, objs] : lost_objects) {
        Map::Id map_id(map_id_str);
        if (map_lost_objects_.count(map_id)) {
            map_lost_objects_[map_id] = std::move(objs);
        }
    }

    next_lost_object_id_ = next_lost_object_id;
}

}  // namespace model
