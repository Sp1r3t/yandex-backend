#include <catch2/catch_test_macros.hpp>

#include "../src/model.h"

using Ms = std::chrono::milliseconds;

SCENARIO("Game model loot generation") {

    GIVEN("a game with a map having a horizontal road and 3 loot types") {
        model::Game game;
        // Deterministic: probability=1.0 with random_gen returning 1.0
        // guarantees loot_shortage items are always generated
        game.SetLootGeneratorConfig(Ms{1000}, 1.0, [] { return 1.0; });

        model::Map map{model::Map::Id{"map1"}, "Map 1"};
        map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 40});
        map.SetLootTypesCount(3);
        game.AddMap(std::move(map));

        WHEN("one player joins and a 1000ms tick occurs") {
            const auto join = game.JoinPlayer(model::Map::Id{"map1"}, "player");
            REQUIRE(join.has_value());

            game.Tick(1000);

            const auto state = game.GetStateByToken(join->auth_token);
            REQUIRE(state.has_value());

            THEN("exactly one lost object is generated") {
                CHECK(state->lost_objects.size() == 1u);
            }

            THEN("lost object type is in [0, 2]") {
                for (const auto& obj : state->lost_objects) {
                    CHECK(obj.type <= 2u);
                }
            }

            THEN("lost object is on the horizontal road: y == 0, x in [0, 40]") {
                REQUIRE(state->lost_objects.size() == 1u);
                const auto& obj = state->lost_objects.front();
                CHECK(obj.pos.y == 0.0);
                CHECK(obj.pos.x >= 0.0);
                CHECK(obj.pos.x <= 40.0);
            }

            THEN("a second tick with loot count == player count produces no new loot") {
                const size_t count_before = state->lost_objects.size();
                game.Tick(1000);
                const auto state2 = game.GetStateByToken(join->auth_token);
                REQUIRE(state2.has_value());
                CHECK(state2->lost_objects.size() == count_before);
            }
        }

        WHEN("two players join and a tick occurs") {
            const auto j1 = game.JoinPlayer(model::Map::Id{"map1"}, "player1");
            const auto j2 = game.JoinPlayer(model::Map::Id{"map1"}, "player2");
            REQUIRE(j1.has_value());
            REQUIRE(j2.has_value());

            game.Tick(1000);

            const auto state = game.GetStateByToken(j1->auth_token);
            REQUIRE(state.has_value());

            THEN("two lost objects are generated") {
                CHECK(state->lost_objects.size() == 2u);
            }

            THEN("all generated lost objects have unique IDs") {
                const auto& objs = state->lost_objects;
                for (size_t i = 0; i < objs.size(); ++i) {
                    for (size_t j = i + 1; j < objs.size(); ++j) {
                        CHECK(objs[i].id != objs[j].id);
                    }
                }
            }

            THEN("all lost object types are in [0, 2]") {
                for (const auto& obj : state->lost_objects) {
                    CHECK(obj.type <= 2u);
                }
            }
        }

        WHEN("a tick occurs with no players, then a player joins and another tick occurs") {
            // First tick: no players → time accumulates, no loot generated
            game.Tick(500);

            const auto join = game.JoinPlayer(model::Map::Id{"map1"}, "player");
            REQUIRE(join.has_value());

            // Second tick: 1 player, accumulated time 1000ms total → loot generated
            game.Tick(500);

            const auto state = game.GetStateByToken(join->auth_token);
            REQUIRE(state.has_value());

            THEN("loot is generated based on accumulated time") {
                CHECK(state->lost_objects.size() == 1u);
            }
        }
    }

    GIVEN("a game with a map having a vertical road and 2 loot types") {
        model::Game game;
        game.SetLootGeneratorConfig(Ms{1000}, 1.0, [] { return 1.0; });

        model::Map map{model::Map::Id{"map1"}, "Map 1"};
        map.AddRoad(model::Road{model::Road::VERTICAL, {10, 0}, 20});
        map.SetLootTypesCount(2);
        game.AddMap(std::move(map));

        WHEN("one player joins and a tick occurs") {
            const auto join = game.JoinPlayer(model::Map::Id{"map1"}, "player");
            REQUIRE(join.has_value());
            game.Tick(1000);
            const auto state = game.GetStateByToken(join->auth_token);
            REQUIRE(state.has_value());

            THEN("lost object is on the vertical road: x == 10, y in [0, 20]") {
                REQUIRE(state->lost_objects.size() == 1u);
                const auto& obj = state->lost_objects.front();
                CHECK(obj.pos.x == 10.0);
                CHECK(obj.pos.y >= 0.0);
                CHECK(obj.pos.y <= 20.0);
            }

            THEN("lost object type is in [0, 1]") {
                for (const auto& obj : state->lost_objects) {
                    CHECK(obj.type <= 1u);
                }
            }
        }
    }

    GIVEN("a game with a map having no loot types count set") {
        model::Game game;
        game.SetLootGeneratorConfig(Ms{1000}, 1.0, [] { return 1.0; });

        model::Map map{model::Map::Id{"map1"}, "Map 1"};
        map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 10});
        // loot_types_count defaults to 0 — no loot types defined
        game.AddMap(std::move(map));

        WHEN("a player joins and a tick occurs") {
            const auto join = game.JoinPlayer(model::Map::Id{"map1"}, "player");
            REQUIRE(join.has_value());
            game.Tick(1000);
            const auto state = game.GetStateByToken(join->auth_token);
            REQUIRE(state.has_value());

            THEN("the game does not crash and state is valid") {
                // Generated loot type would be 0, but loot_types_count==0 means
                // no valid types — we just verify the call doesn't crash
                CHECK(state->players.size() == 1u);
            }
        }
    }
}
