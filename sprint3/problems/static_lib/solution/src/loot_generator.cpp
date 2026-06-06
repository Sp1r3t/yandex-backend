#include "loot_generator.h"

#include <cmath>

namespace loot_gen {

unsigned LootGenerator::Generate(TimeInterval time_delta, unsigned loot_count,
                                 unsigned looter_count) {
    time_without_loot_ += time_delta;
    const unsigned loot_shortage = loot_count > looter_count ? 0u : looter_count - loot_count;
    if (loot_shortage == 0) {
        return 0;
    }
    const double ratio = std::chrono::duration<double>{time_without_loot_} / base_interval_;
    const double probability = 1.0 - std::pow(1.0 - probability_, ratio);
    if (random_generator_() < probability) {
        time_without_loot_ = {};
        return loot_shortage;
    }
    return 0;
}

} // namespace loot_gen
