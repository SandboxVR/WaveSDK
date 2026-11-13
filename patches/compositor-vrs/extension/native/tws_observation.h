#pragma once
#include <cstdint>

// The hook publishes one acquire/release atomic word. A native sample must be
// newer than resume validation, finite in the counter domain, and <=250 ms old.
inline int decodeTwsObservation(uint64_t packed, uint64_t resumeTicks,
                                uint64_t nowTicks, uint64_t frequency) {
    const uint64_t sampleTicks = packed >> 2;
    const unsigned state = static_cast<unsigned>(packed & 3u);
    if (frequency == 0 || nowTicks >= (uint64_t{1} << 62) ||
        sampleTicks <= resumeTicks || sampleTicks > nowTicks ||
        nowTicks - sampleTicks > frequency / 4 || (state != 1 && state != 2)) return -1;
    return static_cast<int>(state - 1);
}
