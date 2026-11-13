#include "../native/tws_observation.h"
#include <cassert>
#include <iostream>
int main() {
    constexpr uint64_t hz = 19200000, resume = hz * 10, sample = resume + 10;
    const auto off = (sample << 2) | 1, on = (sample << 2) | 2;
    assert(decodeTwsObservation(0, resume, sample, hz) == -1);
    assert(decodeTwsObservation(on, sample, sample, hz) == -1); // Previous resume.
    assert(decodeTwsObservation(on, resume, sample - 1, hz) == -1); // Future clock.
    assert(decodeTwsObservation(on, resume, sample, hz) == 1);
    assert(decodeTwsObservation(off, resume, sample + hz / 4, hz) == 0);
    assert(decodeTwsObservation(on, resume, sample + hz / 4 + 1, hz) == -1);
    assert(decodeTwsObservation(off, resume, sample + hz, hz) == -1); // Frozen sample.
    assert(decodeTwsObservation(sample << 2, resume, sample, hz) == -1);
    assert(decodeTwsObservation((sample << 2) | 3, resume, sample, hz) == -1);
    assert(decodeTwsObservation(on, resume, sample, 0) == -1);
    assert(decodeTwsObservation(on, resume, uint64_t{1} << 62, hz) == -1);
    std::cout << "TWS observation freshness tests passed\n";
}
