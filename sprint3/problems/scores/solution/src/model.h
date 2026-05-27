#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "loot_generator.h"
#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct PointD {
    double x = 0.0;
    double y = 0.0;
};

struct Speed2D {
    double vx = 0.0;
    double vy = 0.0;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

enum class MoveCommand {
    LEFT,
    RIGHT,
    UP,
    DOWN,
    STOP
};

struct LostObject {
    unsigned id;
    unsigned type;
    PointD pos;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    static constexpr unsigned kDefaultBagCapacity = 3;

    Map(Id id, std::string name, double dog_speed = 1.0, unsigned bag_capacity = kDefaultBagCapacity) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , dog_speed_(dog_speed)
        , bag_capacity_(bag_capacity) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    double GetDogSpeed() const noexcept {
        return dog_speed_;
    }

    unsigned GetBagCapacity() const noexcept {
        return bag_capacity_;
    }

    void SetBagCapacity(unsigned capacity) noexcept {
        bag_capacity_ = capacity;
    }

    void SetLootTypesCount(unsigned count) noexcept {
        loot_types_count_ = count;
    }

    unsigned GetLootTypesCount() const noexcept {
        return loot_types_count_;
    }

    void SetLootTypeValues(std::vector<unsigned> values) {
        loot_type_values_ = std::move(values);
    }

    unsigned GetLootTypeValue(unsigned type) const noexcept {
        if (type < loot_type_values_.size()) {
            return loot_type_values_[type];
        }
        return 0;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_ = 1.0;
    unsigned bag_capacity_ = kDefaultBagCapacity;
    unsigned loot_types_count_ = 0;
    std::vector<unsigned> loot_type_values_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:
    Dog(std::string name, PointD position)
        : name_(std::move(name))
        , position_(position) {
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    PointD GetPosition() const noexcept {
        return position_;
    }

    Speed2D GetSpeed() const noexcept {
        return speed_;
    }

    Direction GetDirection() const noexcept {
        return direction_;
    }

    void SetPosition(PointD position) noexcept {
        position_ = position;
    }

    void SetSpeed(Speed2D speed) noexcept {
        speed_ = speed;
    }

    void SetDirection(Direction direction) noexcept {
        direction_ = direction;
    }

    const std::vector<LostObject>& GetBag() const noexcept {
        return bag_;
    }

    size_t GetBagSize() const noexcept {
        return bag_.size();
    }

    void AddToBag(LostObject item) {
        bag_.push_back(std::move(item));
    }

    void ClearBag() {
        bag_.clear();
    }

    unsigned GetScore() const noexcept {
        return score_;
    }

    void AddScore(unsigned value) noexcept {
        score_ += value;
    }

private:
    std::string name_;
    PointD position_{};
    Speed2D speed_{};
    Direction direction_ = Direction::NORTH;
    std::vector<LostObject> bag_;
    unsigned score_ = 0;
};

class Player {
public:
    using Id = std::uint32_t;

    Player(Id id, Map::Id map_id, Dog dog)
        : id_(id)
        , map_id_(std::move(map_id))
        , dog_(std::move(dog)) {
    }

    Id GetId() const noexcept {
        return id_;
    }

    const Map::Id& GetMapId() const noexcept {
        return map_id_;
    }

    const Dog& GetDog() const noexcept {
        return dog_;
    }

    Dog& GetDog() noexcept {
        return dog_;
    }

private:
    Id id_;
    Map::Id map_id_;
    Dog dog_;
};

class Game {
public:
    using Maps = std::vector<Map>;
    using TimeInterval = loot_gen::LootGenerator::TimeInterval;

    Game() = default;
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&& other) noexcept;
    Game& operator=(Game&& other) noexcept;

    struct JoinGameResult {
        Player::Id player_id;
        std::string auth_token;
    };

    struct PlayerInfo {
        Player::Id id;
        Map::Id map_id;
        std::string name;
    };

    struct PlayerSnapshot {
        Player::Id id;
        PointD position;
        Speed2D speed;
        Direction direction;
        std::vector<LostObject> bag;
        unsigned score = 0;
    };

    struct GameState {
        std::vector<PlayerSnapshot> players;
        std::vector<LostObject> lost_objects;
    };

    void SetLootGeneratorConfig(TimeInterval period, double probability,
                                loot_gen::LootGenerator::RandomGenerator random_gen = {});

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    std::optional<JoinGameResult> JoinPlayer(const Map::Id& map_id, std::string user_name) const;
    std::optional<PlayerInfo> FindPlayerByToken(std::string_view token) const;
    std::vector<PlayerInfo> GetPlayersOnMap(const Map::Id& map_id) const;
    std::optional<GameState> GetStateByToken(std::string_view token) const;
    bool SetPlayerAction(std::string_view token, MoveCommand move) const;
    void Tick(std::int64_t time_delta_ms) const;

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using TokenToPlayerIndex = std::unordered_map<std::string, size_t>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;

    TimeInterval loot_period_{5000};
    double loot_probability_ = 0.5;
    loot_gen::LootGenerator::RandomGenerator loot_random_gen_;

    mutable std::mutex players_mutex_;
    mutable std::vector<Player> players_;
    mutable TokenToPlayerIndex token_to_player_index_;
    mutable Player::Id next_player_id_ = 0;

    mutable std::unordered_map<Map::Id, loot_gen::LootGenerator, MapIdHasher> map_generators_;
    mutable std::unordered_map<Map::Id, std::vector<LostObject>, MapIdHasher> map_lost_objects_;
    mutable unsigned next_lost_object_id_ = 0;
};

}  // namespace model
