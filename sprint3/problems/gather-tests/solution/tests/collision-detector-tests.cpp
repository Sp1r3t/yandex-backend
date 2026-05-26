#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("No gatherers - no events") {
    TestProvider provider{{Item{{0, 0}, 0.5}}, {}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("No items - no events") {
    TestProvider provider{{}, {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Stationary gatherer does not collect") {
    TestProvider provider{
        {Item{{5, 0}, 0.5}},
        {Gatherer{{5, 0}, {5, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item far from path - not collected") {
    TestProvider provider{
        {Item{{5, 10.0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item just outside combined radius - not collected") {
    // combined radius = 0.5 + 0.5 = 1.0, item at perpendicular distance 1.1
    TestProvider provider{
        {Item{{5, 1.1}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item behind gatherer start - not collected") {
    TestProvider provider{
        {Item{{-1, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item past gatherer end - not collected") {
    TestProvider provider{
        {Item{{11, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item on path - collected with correct event data") {
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

TEST_CASE("Item within gatherer width only (zero item width) - collected") {
    // Catches Wrong2: uses IsCollected(item.width) instead of IsCollected(gatherer.width + item.width)
    // distance=0.8, gatherer.width=1.0, item.width=0.0
    // Correct:  0.64 <= 1.0^2  -> collected
    // Wrong2:   0.64 <= 0.0^2  -> NOT collected
    TestProvider provider{
        {Item{{5, 0.8}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 1.0}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
}

TEST_CASE("Item within item width only (zero gatherer width) - collected") {
    // Catches Wrong1: uses IsCollected(gatherer.width) instead of IsCollected(gatherer.width + item.width)
    // distance=0.8, gatherer.width=0.0, item.width=1.0
    // Correct:  0.64 <= 1.0^2  -> collected
    // Wrong1:   0.64 <= 0.0^2  -> NOT collected
    TestProvider provider{
        {Item{{5, 0.8}, 1.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.0}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
}

TEST_CASE("Item within combined radius but outside each individual radius - collected") {
    // distance=0.7, gatherer.width=0.5, item.width=0.5
    // Correct:  0.49 <= 1.0^2  -> collected
    // Wrong1:   0.49 <= 0.5^2  -> NOT collected
    // Wrong2:   0.49 <= 0.5^2  -> NOT collected
    TestProvider provider{
        {Item{{5, 0.7}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
}

TEST_CASE("Events sorted in chronological order") {
    // Items listed in reverse order to expose missing sort.
    // item[0] at x=7 (time=0.7), item[1] at x=3 (time=0.3)
    // Correct:  events = [time=0.3, time=0.7]
    // Wrong3:   events = [time=0.7, time=0.3]  (unsorted)
    TestProvider provider{
        {Item{{7, 0}, 0.0}, Item{{3, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    CHECK(events[0].time <= events[1].time + EPS);
    CHECK(events[0].item_id == 1);
    CHECK(events[1].item_id == 0);
    CHECK(events[0].time == Catch::Approx(0.3).margin(EPS));
    CHECK(events[1].time == Catch::Approx(0.7).margin(EPS));
}

TEST_CASE("Diagonal path - item exactly on path - collected") {
    // Catches Wrong4/Wrong5: they use only the x or y offset instead of
    // the true perpendicular distance, giving sq_distance=9 for (3,3) on (0,0)->(10,10).
    // Correct:   sq_distance=0, time=0.3  -> collected with radius 0.5
    // Wrong4/5:  sq_distance=9            -> NOT collected
    TestProvider provider{
        {Item{{3, 3}, 0.0}},
        {Gatherer{{0, 0}, {10, 10}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == Catch::Approx(0.0).margin(EPS));
    CHECK(events[0].time == Catch::Approx(0.3).margin(EPS));
}

TEST_CASE("Diagonal path - item off path - not collected") {
    TestProvider provider{
        {Item{{0, 5}, 0.0}},
        {Gatherer{{0, 0}, {10, 10}, 0.5}}};
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Correct sq_distance value in event") {
    // Item at (5, 0.6), path along x-axis: sq_distance = 0.6^2 = 0.36
    TestProvider provider{
        {Item{{5, 0.6}, 0.5}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].sq_distance == Catch::Approx(0.36).margin(EPS));
}

TEST_CASE("Multiple gatherers - correct gatherer_id in event") {
    TestProvider provider{
        {Item{{5, 0}, 0.0}},
        {Gatherer{{0, 2}, {10, 2}, 0.5},   // gatherer 0 - too far above item
         Gatherer{{0, 0}, {10, 0}, 0.5}}};  // gatherer 1 - collects item
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].gatherer_id == 1);
    CHECK(events[0].item_id == 0);
}

TEST_CASE("Multiple items on path - all collected") {
    TestProvider provider{
        {Item{{2, 0}, 0.0}, Item{{5, 0}, 0.0}, Item{{8, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
}

TEST_CASE("Item collected by multiple gatherers - both events present") {
    TestProvider provider{
        {Item{{5, 0}, 0.0}},
        {Gatherer{{0, 0}, {10, 0}, 0.5},
         Gatherer{{10, 0}, {0, 0}, 0.5}}};
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
}
