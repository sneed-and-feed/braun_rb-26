#pragma once

#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"

#include <atomic>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>

namespace rb26 {

struct Rb26Parameters {
    // Input / Pre-Delay
    float preDelayMs = 20.0f;       // 0.0 - 500.0 ms
    float dryWetMix = 0.35f;        // 0.0 - 1.0 (0% - 100%)
    float earlyLateMix = 0.50f;     // 0.0 - 1.0 (0% - 100%)
    
    // Low-End Decoupled Engine
    float lowCrossoverHz = 180.0f;  // 60.0 - 400.0 Hz
    float bassRt60Mult = 1.0f;      // 0.2 - 4.0x
    float punchDucking = 0.40f;     // 0.0 - 1.0
    float subMonoHz = 120.0f;       // 20.0 - 250.0 Hz
    
    // Reverb Tank (FDN)
    float roomSize = 0.65f;         // 0.1 - 2.0
    float decayRt60Sec = 3.5f;      // 0.2 - 30.0 s
    float highDampingHz = 6500.0f;  // 1000.0 - 20000.0 Hz
    float diffusionDensity = 0.75f; // 0.0 - 1.0
    bool freezeHold = false;        // true/false
    
    // Bidirectional Pitch Diffusion
    float shimmerSend = 0.30f;      // 0.0 - 1.0
    float dimmerSend = 0.25f;       // 0.0 - 1.0
    int shimmerInterval = 12;       // +7, +12, +24 semitones
    int dimmerInterval = -12;       // -12, -24 semitones
    float pitchBlend = 0.0f;        // -1.0 (Dimmer) to +1.0 (Shimmer)
    float pitchFeedback = 0.50f;    // 0.0 - 0.95
    
    // Tail-Level Pitch Modulation
    float tailModRateHz = 0.85f;    // 0.05 - 5.0 Hz
    float tailModDepthMs = 1.2f;    // 0.0 - 5.0 ms
    float tailBloomMs = 85.0f;      // 20.0 - 300.0 ms
    
    // Master Bus
    float stereoWidth = 1.0f;       // 0.0 - 2.0
    float outputTrimDb = 0.0f;      // -24.0 - +12.0 dB
    bool limiterEnable = true;      // true/false
};

struct PresetDefinition {
    const char* id;
    const char* name;
    const char* category;
    const char* description;
    Rb26Parameters params;
};

class Rb26ReverbEngine {
public:
    Rb26ReverbEngine() noexcept;
    ~Rb26ReverbEngine() noexcept = default;

    // Curated factory presets
    static std::vector<PresetDefinition> getFactoryPresets();

    // Hard real-time lifecycle
    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void setParameters(const Rb26Parameters& params) noexcept;

    // Guaranteed hard real-time safe: 0 dynamic allocations, 0 locks
    void process(const float* const* inputChannels,
                 float* const* outputChannels,
                 int numChannels,
                 int numSamples) noexcept;

    // Telemetry for CRT display (lock-free SPSC)
    struct VisualizerFrame {
        float inputRmsL { 0.0f }, inputRmsR { 0.0f };
        float outputRmsL { 0.0f }, outputRmsR { 0.0f };
        float correlation { 0.0f };
        float lowEnergy { 0.0f }, midEnergy { 0.0f }, highEnergy { 0.0f };
        float decayEnvelope { 0.0f };
    };
    bool popVisualizerFrame(VisualizerFrame& frame) noexcept;

private:
    double mSampleRate { 48000.0 };
    int mMaxBlockSize { 512 };
    Rb26Parameters mParams;

    // Pre-Delay Circular Buffer (500 ms at 192 kHz = 96,000 samples -> 131,072)
    static constexpr size_t kPreDelayBufferCapacity = 131072;
    static constexpr size_t kPreDelayBufferMask = kPreDelayBufferCapacity - 1;
    std::vector<float> mPreDelayBufferL;
    std::vector<float> mPreDelayBufferR;
    size_t mPreDelayWriteIndex { 0 };
    OnePoleSmoother mPreDelaySmoother;

    // Sub-modules
    LowBandModalMatrix mLowBandMatrix;
    EarlyReflections mEarlyReflections;
    FdnReverbTank mFdnTank;
    PitchShifter mPitchShifter;

    // Master Bus Parameter Smoothers
    OnePoleSmoother mDryWetSmoother;
    OnePoleSmoother mEarlyLateSmoother;
    OnePoleSmoother mStereoWidthSmoother;
    OnePoleSmoother mOutputTrimSmoother;

    // Pitch feedback state between FDN tank and PitchShifter
    float mLastPitchFbL { 0.0f };
    float mLastPitchFbR { 0.0f };

    // Lock-Free SPSC Telemetry Queue
    static constexpr size_t kTelemetryQueueCapacity = 16;
    static constexpr size_t kTelemetryQueueMask = kTelemetryQueueCapacity - 1;
    std::array<VisualizerFrame, kTelemetryQueueCapacity> mTelemetryQueue;
    std::atomic<size_t> mTelemetryWriteIndex { 0 };
    std::atomic<size_t> mTelemetryReadIndex { 0 };

    // Telemetry metering accumulators
    float mInputRmsSumL { 0.0f }, mInputRmsSumR { 0.0f };
    float mOutputRmsSumL { 0.0f }, mOutputRmsSumR { 0.0f };
    float mCorrelationSum { 0.0f };
    float mDecayPeakFollower { 0.0f };
    int mTelemetryDecimator { 0 };

    void pushVisualizerFrame(const VisualizerFrame& frame) noexcept;
};

} // namespace rb26
