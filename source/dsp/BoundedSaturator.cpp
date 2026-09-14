#include "BoundedSaturator.h"

namespace rb26 {

void BoundedSaturator::processBlock(const float* input, float* output, int numSamples) const noexcept {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

void BoundedSaturator::processBlock(float* buffer, int numSamples) const noexcept {
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = processSample(buffer[i]);
    }
}

} // namespace rb26
