#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

namespace {

using namespace collision_detector;

class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }
    Item GetItem(size_t idx) const override {
        return items_[idx];
    }
    size_t GatherersCount() const override {
        return gatherers_.size();
    }
    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_[idx];
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

constexpr double EPS = 1e-10;

}  // namespace

// ─── No events scenarios ─────────────────────────────────────────────────────

TEST_CASE("Empty provider produces no events") {
    TestProvider provider({}, {});
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("No gatherers - no events") {
    TestProvider provider(
        {{{5.0, 5.0}, 1.0}},
        {}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("No items - no events") {
    TestProvider provider(
        {},
        {{{0.0, 0.0}, {10.0, 0.0}, 1.0}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Stationary gatherer produces no events") {
    // Gatherer at (5,0) → (5,0): not moved, should be skipped
    TestProvider provider(
        {{{5.0, 0.0}, 1.0}},
        {{{5.0, 0.0}, {5.0, 0.0}, 1.0}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item behind gatherer start produces no event") {
    // Gatherer (0,0)→(10,0), item at (-5,0): proj_ratio = -0.5
    TestProvider provider(
        {{{-5.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item past gatherer end produces no event") {
    // Gatherer (0,0)→(10,0), item at (15,0): proj_ratio = 1.5
    TestProvider provider(
        {{{15.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item too far laterally produces no event") {
    // Gatherer (0,0)→(10,0), item at (5,3): sq_dist=9, collect_radius=1 → miss
    TestProvider provider(
        {{{5.0, 3.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item just outside collect radius produces no event") {
    // collect_radius = 0.5 + 0.5 = 1.0; item at (5, 1.0001): sq_dist ≈ 1.00020001 > 1
    TestProvider provider(
        {{{5.0, 1.0001}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    CHECK(FindGatherEvents(provider).empty());
}

// ─── Detection scenarios ──────────────────────────────────────────────────────

TEST_CASE("Direct hit on path - event detected") {
    // Item directly on movement line at midpoint
    TestProvider provider(
        {{{5.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK(events[0].sq_distance < EPS);
    CHECK(std::abs(events[0].time - 0.5) < EPS);
}

TEST_CASE("Item at start position - event detected") {
    // Item at start: proj_ratio = 0
    TestProvider provider(
        {{{0.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(std::abs(events[0].time - 0.0) < EPS);
}

TEST_CASE("Item at end position - event detected") {
    // Item at end: proj_ratio = 1
    TestProvider provider(
        {{{10.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(std::abs(events[0].time - 1.0) < EPS);
}

TEST_CASE("Item exactly at collect radius boundary - event detected") {
    // collect_radius = 0.5 + 0.5 = 1.0; item at (5, 1.0): sq_dist = 1.0 ≤ 1.0² → collected
    TestProvider provider(
        {{{5.0, 1.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(std::abs(events[0].sq_distance - 1.0) < EPS);
}

TEST_CASE("Diagonal movement - item on diagonal path") {
    // Gatherer (0,0)→(10,10), item at (5,5) directly on path
    TestProvider provider(
        {{{5.0, 5.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 10.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance < EPS);
    CHECK(std::abs(events[0].time - 0.5) < EPS);
}

// ─── Chronological ordering ───────────────────────────────────────────────────

TEST_CASE("Events are sorted by time") {
    // Items at x=7 and x=3; gatherer moves (0,0)→(10,0)
    // Item 0 (x=7) encountered at t=0.7, Item 1 (x=3) at t=0.3
    TestProvider provider(
        {{{7.0, 0.0}, 0.5}, {{3.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    CHECK(events[0].time <= events[1].time);
    CHECK(events[0].item_id == 1);  // x=3 comes first
    CHECK(events[1].item_id == 0);  // x=7 comes second
}

TEST_CASE("Three items are sorted chronologically") {
    TestProvider provider(
        {{{9.0, 0.0}, 0.5}, {{1.0, 0.0}, 0.5}, {{5.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i - 1].time <= events[i].time);
    }
    CHECK(events[0].item_id == 1);  // t=0.1
    CHECK(events[1].item_id == 2);  // t=0.5
    CHECK(events[2].item_id == 0);  // t=0.9
}

// ─── Correct event data ───────────────────────────────────────────────────────

TEST_CASE("Correct sq_distance for off-axis item") {
    // Gatherer (0,0)→(10,0), item at (5,2): sq_dist = 4
    // collect_radius = 0.5 + 2.5 = 3.0 → sq_dist(4) ≤ 9 → collected
    TestProvider provider(
        {{{5.0, 2.0}, 2.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(std::abs(events[0].sq_distance - 4.0) < EPS);
}

TEST_CASE("Correct time for item at 30% of path") {
    // Gatherer (0,0)→(10,0), item at (3,0): time = 0.3
    TestProvider provider(
        {{{3.0, 0.0}, 0.5}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.5}}
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(std::abs(events[0].time - 0.3) < EPS);
}

TEST_CASE("Correct indices in event") {
    // 3 items, 2 gatherers; only specific combinations should collide
    // Gatherer 0: (0,0)→(10,0) collects item 1 at (5,0)
    // Gatherer 1: (0,10)→(10,10) collects item 2 at (5,10)
    // Item 0 at (5, 100) is not collected by anyone
    TestProvider provider(
        {{{5.0, 100.0}, 0.5}, {{5.0, 0.0}, 0.5}, {{5.0, 10.0}, 0.5}},
        {
            {{0.0, 0.0}, {10.0, 0.0}, 0.5},
            {{0.0, 10.0}, {10.0, 10.0}, 0.5}
        }
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);

    bool found_g0_i1 = false, found_g1_i2 = false;
    for (const auto& e : events) {
        if (e.gatherer_id == 0 && e.item_id == 1) found_g0_i1 = true;
        if (e.gatherer_id == 1 && e.item_id == 2) found_g1_i2 = true;
    }
    CHECK(found_g0_i1);
    CHECK(found_g1_i2);
}

// ─── Multi-gatherer scenarios ─────────────────────────────────────────────────

TEST_CASE("Same item collected by two gatherers - both events present") {
    // Two identical gatherers both collect the same item
    TestProvider provider(
        {{{5.0, 0.0}, 0.5}},
        {
            {{0.0, 0.0}, {10.0, 0.0}, 0.5},
            {{0.0, 0.0}, {10.0, 0.0}, 0.5}
        }
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    CHECK(events[0].item_id == 0);
    CHECK(events[1].item_id == 0);
}

TEST_CASE("Multiple gatherers collect separate items") {
    // Gatherer 0 goes right, gatherer 1 goes left; items on opposite sides
    TestProvider provider(
        {{{8.0, 0.0}, 0.5}, {{-8.0, 0.0}, 0.5}},
        {
            {{0.0, 0.0}, {10.0, 0.0}, 0.5},
            {{0.0, 0.0}, {-10.0, 0.0}, 0.5}
        }
    );
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);

    bool found_g0_i0 = false, found_g1_i1 = false;
    for (const auto& e : events) {
        if (e.gatherer_id == 0 && e.item_id == 0) found_g0_i0 = true;
        if (e.gatherer_id == 1 && e.item_id == 1) found_g1_i1 = true;
        // Wrong cross-collections must not appear
        CHECK(!(e.gatherer_id == 0 && e.item_id == 1));
        CHECK(!(e.gatherer_id == 1 && e.item_id == 0));
    }
    CHECK(found_g0_i0);
    CHECK(found_g1_i1);
}
