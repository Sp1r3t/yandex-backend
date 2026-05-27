#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

const double EPS = 1e-10;

}  // namespace

// ─── No events scenarios ─────────────────────────────────────────────────────

TEST_CASE("Empty provider produces no events") {
    TestProvider provider({}, {});
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("No gatherers - no events") {
    TestProvider provider{{Item{{0, 0}, 0.5}}, {}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("No items - no events") {
    TestProvider provider{{}, {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Stationary gatherer does not collect") {
    TestProvider provider{
        {Item{{5, 0}, 0.5}},
        {Gatherer{{5, 0}, {5, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item far from path - not collected") {
    // perpendicular distance 10.0, combined radius 0.5 → miss
    TestProvider provider{
        {Item{{5, 10.0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item just outside combined radius - not collected") {
    // combined radius = 0.5 + 0.5 = 1.0, perpendicular distance 1.1
    TestProvider provider{
        {Item{{5, 1.1}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item behind gatherer start - not collected") {
    // proj_ratio = -0.1 < 0
    TestProvider provider{
        {Item{{-1, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item past gatherer end - not collected") {
    // proj_ratio = 1.1 > 1
    TestProvider provider{
        {Item{{11, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

TEST_CASE("Item just outside collect radius produces no event") {
    // combined radius = 1.0; perpendicular distance 1.0001 → miss
    TestProvider provider{
        {Item{{5, 1.0001}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    CHECK(FindGatherEvents(provider).empty());
}

// ─── Detection scenarios ──────────────────────────────────────────────────────

TEST_CASE("Item on path - collected with correct event data") {
    // Item directly on path at midpoint
    TestProvider provider{
        {Item{{5, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK(events[0].sq_distance == Catch::Approx(0.0).margin(EPS));
    CHECK(events[0].time == Catch::Approx(0.5).margin(EPS));
}

TEST_CASE("Item at start position - collected") {
    // proj_ratio = 0
    TestProvider provider{
        {Item{{0, 0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].time == Catch::Approx(0.0).margin(EPS));
}

TEST_CASE("Item at end position - collected") {
    // proj_ratio = 1
    TestProvider provider{
        {Item{{10, 0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].time == Catch::Approx(1.0).margin(EPS));
}

TEST_CASE("Item exactly at collect radius boundary - collected") {
    // combined radius = 1.0; item at perpendicular distance exactly 1.0
    TestProvider provider{
        {Item{{5, 1.0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == Catch::Approx(1.0).margin(EPS));
}

TEST_CASE("Diagonal movement - item on diagonal path") {
    // Gatherer (0,0)→(10,10), item at (5,5) directly on path
    TestProvider provider{
        {Item{{5, 5}, 0.5}},
        {Gatherer{{0, 0}, {10, 10}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == Catch::Approx(0.0).margin(EPS));
    CHECK(events[0].time == Catch::Approx(0.5).margin(EPS));
}

TEST_CASE("Item within gatherer width only (zero item width) - collected") {
    // Catches wrong impl that uses IsCollected(item.width) only
    // distance=0.8, gatherer.width=1.0, item.width=0.0
    // Correct:  0.64 <= 1.0^2  → collected
    // Wrong:    0.64 <= 0.0^2  → not collected
    TestProvider provider{
        {Item{{5, 0.8}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 1.0}}};
    CHECK(FindGatherEvents(provider).size() == 1);
}

TEST_CASE("Item within item width only (zero gatherer width) - collected") {
    // Catches wrong impl that uses IsCollected(gatherer.width) only
    // distance=0.8, gatherer.width=0.0, item.width=1.0
    // Correct:  0.64 <= 1.0^2  → collected
    // Wrong:    0.64 <= 0.0^2  → not collected
    TestProvider provider{
        {Item{{5, 0.8}, 1.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.0}}};
    CHECK(FindGatherEvents(provider).size() == 1);
}

// ─── Chronological ordering ───────────────────────────────────────────────────

TEST_CASE("Events sorted in chronological order") {
    // Items listed in reverse time order to expose missing sort
    // item[0] at x=7 (time=0.7), item[1] at x=3 (time=0.3)
    TestProvider provider{
        {Item{{7, 0}, 0.0}, Item{{3, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    CHECK(events[0].time <= events[1].time);
    CHECK(events[0].item_id == 1);
    CHECK(events[1].item_id == 0);
}

TEST_CASE("Three items sorted chronologically") {
    TestProvider provider{
        {Item{{9, 0}, 0.0}, Item{{1, 0}, 0.0}, Item{{5, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i - 1].time <= events[i].time);
    }
    CHECK(events[0].item_id == 1);  // x=1, t=0.1
    CHECK(events[1].item_id == 2);  // x=5, t=0.5
    CHECK(events[2].item_id == 0);  // x=9, t=0.9
}

// ─── Correct event data ───────────────────────────────────────────────────────

TEST_CASE("Correct sq_distance for off-axis item") {
    // Gatherer (0,0)→(10,0), item at (5,2): sq_dist = 4
    // collect_radius = 0.5 + 2.5 = 3.0 → collected
    TestProvider provider{
        {Item{{5, 2}, 2.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == Catch::Approx(4.0).margin(EPS));
}

TEST_CASE("Correct time for item at 30% of path") {
    // Gatherer (0,0)→(10,0), item at (3,0): time = 0.3
    TestProvider provider{
        {Item{{3, 0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].time == Catch::Approx(0.3).margin(EPS));
}

TEST_CASE("Correct indices in event") {
    // Gatherer 0 collects item 1, gatherer 1 collects item 2; item 0 missed
    TestProvider provider{
        {Item{{5, 100}, 0.5}, Item{{5, 0}, 0.5}, Item{{5, 10}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}, Gatherer{{0, 10}, {10, 10}, 0.5}}};
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

TEST_CASE("Same item collected by two gatherers - both events recorded") {
    TestProvider provider{
        {Item{{5, 0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}, Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
}

TEST_CASE("Multiple gatherers collect separate items without cross-collection") {
    // Gatherer 0 goes right (collects item 0), gatherer 1 goes left (collects item 1)
    TestProvider provider{
        {Item{{8, 0}, 0.5}, Item{{-8, 0}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}, Gatherer{{0, 0}, {-10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    bool found_g0_i0 = false, found_g1_i1 = false;
    for (const auto& e : events) {
        if (e.gatherer_id == 0 && e.item_id == 0) found_g0_i0 = true;
        if (e.gatherer_id == 1 && e.item_id == 1) found_g1_i1 = true;
        CHECK(!(e.gatherer_id == 0 && e.item_id == 1));
        CHECK(!(e.gatherer_id == 1 && e.item_id == 0));
    }
    CHECK(found_g0_i0);
    CHECK(found_g1_i1);
}
