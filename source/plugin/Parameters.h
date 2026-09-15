#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/Rb26Engine.h"
#include <vector>
#include <memory>
#include <atomic>
#include <array>
#include <string>
#include <cmath>

namespace rb26 {

namespace ParamIDs {
    inline const juce::ParameterID inputTrimDb       { "input_trim_db", 1 };
    inline const juce::ParameterID preDelayMs        { "pre_delay_ms", 1 };
    inline const juce::ParameterID dryWetMix         { "dry_wet_mix", 1 };
    inline const juce::ParameterID earlyLateMix      { "early_late_mix", 1 };
    inline const juce::ParameterID lowCrossoverHz    { "low_crossover_hz", 1 };
    inline const juce::ParameterID bassRt60Mult      { "bass_rt60_mult", 1 };
    inline const juce::ParameterID punchDucking      { "punch_ducking", 1 };
    inline const juce::ParameterID subMonoHz         { "sub_mono_hz", 1 };
    inline const juce::ParameterID roomSize          { "room_size", 1 };
    inline const juce::ParameterID decayRt60Sec      { "decay_rt60_sec", 1 };
    inline const juce::ParameterID highDampingHz     { "high_damping_hz", 1 };
    inline const juce::ParameterID diffusionDensity  { "diffusion_density", 1 };
    inline const juce::ParameterID freezeHold        { "freeze_hold", 1 };
    inline const juce::ParameterID shimmerSend       { "shimmer_send", 1 };
    inline const juce::ParameterID dimmerSend        { "dimmer_send", 1 };
    inline const juce::ParameterID shimmerInterval   { "shimmer_interval", 1 };
    inline const juce::ParameterID dimmerInterval    { "dimmer_interval", 1 };
    inline const juce::ParameterID pitchBlend        { "pitch_blend", 1 };
    inline const juce::ParameterID pitchFeedback     { "pitch_feedback", 1 };
    inline const juce::ParameterID pitchDelayMs      { "pitch_delay_ms", 1 };
    inline const juce::ParameterID tailModRateHz     { "tail_mod_rate_hz", 1 };
    inline const juce::ParameterID tailModDepthMs    { "tail_mod_depth_ms", 1 };
    inline const juce::ParameterID tailBloomMs       { "tail_bloom_ms", 1 };
    inline const juce::ParameterID stereoWidth       { "stereo_width", 1 };
    inline const juce::ParameterID outputTrimDb      { "output_trim_db", 1 };
    inline const juce::ParameterID limiterEnable     { "limiter_enable", 1 };
}

// ============================================================================
// Choice interval helpers for Shimmer (+7, +12, +24) and Dimmer (-2, -7, -12)
// ============================================================================
inline const juce::StringArray& getShimmerIntervalChoices() {
    static const juce::StringArray choices { "+7 st (Fifth)", "+12 st (Octave)", "+24 st (2 Octaves)" };
    return choices;
}

inline const juce::StringArray& getDimmerIntervalChoices() {
    static const juce::StringArray choices { "-2 st (Dark Chorus)", "-7 st (Sub-Fifth Drone)", "-12 st (Sub-Octave Bloom)" };
    return choices;
}

inline int shimmerIntervalFromIndex(int index) noexcept {
    switch (index) {
        case 0: return 7;
        case 1: return 12;
        case 2: return 24;
        default: return 12;
    }
}

inline int shimmerIndexFromInterval(int semitones) noexcept {
    if (semitones == 7) return 0;
    if (semitones == 24) return 2;
    return 1;
}

inline int dimmerIntervalFromIndex(int index) noexcept {
    switch (index) {
        case 0: return -2;
        case 1: return -7;
        case 2: return -12;
        default: return -12;
    }
}

inline int dimmerIndexFromInterval(int semitones) noexcept {
    if (semitones == -2) return 0;
    if (semitones == -7) return 1;
    return 2;
}

// ============================================================================
// Parameter Metadata Table for 2-Way APVTS / Web / GUI Binding
// ============================================================================
struct ParameterMetadata {
    const char* apvtsId;
    const char* webId;
    const char* name;
    const char* unit;
    float minVal;
    float maxVal;
    float defaultVal;
    bool isBool;
    bool isChoice;
};

inline const std::array<ParameterMetadata, 26>& getParameterMetadataTable() {
    static const std::array<ParameterMetadata, 26> table {{
        { "input_trim_db",      "inputTrimDb",      "Input Trim",            "dB",   -18.0f,  18.0f,    0.0f,   false, false },
        { "pre_delay_ms",       "preDelayMs",       "Pre-Delay",             "ms",   0.0f,    500.0f,   24.0f,  false, false },
        { "dry_wet_mix",        "dryWetMix",        "Dry / Wet Mix",         "%",    0.0f,    1.0f,     0.40f,  false, false },
        { "early_late_mix",     "earlyLateMix",     "Early / Late Mix",      "%",    0.0f,    1.0f,     0.50f,  false, false },
        { "low_crossover_hz",   "lowCrossoverHz",   "Low Crossover Freq",    "Hz",   60.0f,   400.0f,   180.0f, false, false },
        { "bass_rt60_mult",     "bassRt60Mult",     "Bass RT60 Multiplier",  "x",    0.2f,    4.0f,     1.0f,   false, false },
        { "punch_ducking",      "punchDucking",     "Punch Ducking",         "%",    0.0f,    1.0f,     0.65f,  false, false },
        { "sub_mono_hz",        "subMonoHz",        "Sub Mono Freq",         "Hz",   20.0f,   250.0f,   120.0f, false, false },
        { "room_size",          "roomSize",         "Room Size",             "",     0.1f,    2.0f,     1.0f,   false, false },
        { "decay_rt60_sec",     "decayRt60Sec",     "Decay Time (RT60)",     "s",    0.2f,    30.0f,    6.5f,   false, false },
        { "high_damping_hz",    "highDampingHz",    "High Damping Freq",     "Hz",   1000.0f, 20000.0f, 7500.0f,false, false },
        { "diffusion_density",  "diffusionDensity", "Diffusion Density",     "%",    0.0f,    1.0f,     0.75f,  false, false },
        { "freeze_hold",        "freezeHold",       "Freeze Hold",           "",     0.0f,    1.0f,     0.0f,   true,  false },
        { "shimmer_send",       "shimmerSend",      "Shimmer Send",          "%",    0.0f,    1.0f,     0.40f,  false, false },
        { "dimmer_send",        "dimmerSend",       "Dimmer Send",           "%",    0.0f,    1.0f,     0.35f,  false, false },
        { "shimmer_interval",   "shimmerInterval",  "Shimmer Interval",      "st",   0.0f,    2.0f,     1.0f,   false, true  },
        { "dimmer_interval",    "dimmerInterval",   "Dimmer Interval",       "st",   0.0f,    2.0f,     2.0f,   false, true  },
        { "pitch_blend",        "pitchBlend",       "Pitch Blend (Dim/Shim)","",     -1.0f,   1.0f,     0.0f,   false, false },
        { "pitch_feedback",     "pitchFeedback",    "Pitch Feedback",        "%",    0.0f,    0.95f,    0.45f,  false, false },
        { "pitch_delay_ms",     "pitchDelayMs",     "Pitch Delay",           "ms",   20.0f,   500.0f,   150.0f, false, false },
        { "tail_mod_rate_hz",   "tailModRateHz",    "Tail Mod Rate",         "Hz",   0.05f,   5.0f,     0.65f,  false, false },
        { "tail_mod_depth_ms",  "tailModDepthMs",   "Tail Mod Depth",        "ms",   0.0f,    5.0f,     2.25f,  false, false },
        { "tail_bloom_ms",      "tailBloomMs",      "Tail Bloom Attack",     "ms",   20.0f,   300.0f,   85.0f,  false, false },
        { "stereo_width",       "stereoWidth",      "Stereo Width",          "%",    0.0f,    2.0f,     1.0f,   false, false },
        { "output_trim_db",     "outputTrimDb",     "Output Trim",           "dB",   -24.0f,  12.0f,    0.0f,   false, false },
        { "limiter_enable",     "limiterEnable",    "Master Limiter",        "",     0.0f,    1.0f,     1.0f,   true,  false }
    }};
    return table;
}

// ============================================================================
// Lock-Free Plain-Old-Data (POD) Parameter Snapshot Structure
// ============================================================================
struct alignas(16) Rb26ParameterSnapshot {
    float inputTrimDb      { 0.0f };
    float preDelayMs       { 24.0f };
    float dryWetMix        { 0.40f };
    float earlyLateMix     { 0.50f };
    float lowCrossoverHz   { 180.0f };
    float bassRt60Mult     { 1.0f };
    float punchDucking     { 0.65f };
    float subMonoHz        { 120.0f };
    float roomSize         { 1.0f };
    float decayRt60Sec     { 6.5f };
    float highDampingHz    { 7500.0f };
    float diffusionDensity { 0.75f };
    bool  freezeHold       { false };
    float shimmerSend      { 0.40f };
    float dimmerSend       { 0.35f };
    int   shimmerInterval  { 12 };
    int   dimmerInterval   { -12 };
    float pitchBlend       { 0.0f };
    float pitchFeedback    { 0.45f };
    float pitchDelayMs     { 150.0f };
    float tailModRateHz    { 0.65f };
    float tailModDepthMs   { 2.25f };
    float tailBloomMs      { 85.0f };
    float stereoWidth      { 1.0f };
    float outputTrimDb     { 0.0f };
    bool  limiterEnable    { true };

    [[nodiscard]] Rb26Parameters toDspParams() const noexcept {
        Rb26Parameters p;
        p.inputTrimDb      = inputTrimDb;
        p.preDelayMs       = preDelayMs;
        p.dryWetMix        = dryWetMix;
        p.earlyLateMix     = earlyLateMix;
        p.lowCrossoverHz   = lowCrossoverHz;
        p.bassRt60Mult     = bassRt60Mult;
        p.punchDucking     = punchDucking;
        p.subMonoHz        = subMonoHz;
        p.roomSize         = roomSize;
        p.decayRt60Sec     = decayRt60Sec;
        p.highDampingHz    = highDampingHz;
        p.diffusionDensity = diffusionDensity;
        p.freezeHold       = freezeHold;
        p.shimmerSend      = shimmerSend;
        p.dimmerSend       = dimmerSend;
        p.shimmerInterval  = shimmerInterval;
        p.dimmerInterval   = dimmerInterval;
        p.pitchBlend       = pitchBlend;
        p.pitchFeedback    = pitchFeedback;
        p.pitchDelayMs     = pitchDelayMs;
        p.tailModRateHz    = tailModRateHz;
        p.tailModDepthMs   = tailModDepthMs;
        p.tailBloomMs      = tailBloomMs;
        p.stereoWidth      = stereoWidth;
        p.outputTrimDb     = outputTrimDb;
        p.limiterEnable    = limiterEnable;
        return p;
    }
};

// ============================================================================
// Cached Atomic Parameter Pointers for Real-Time Lock-Free Snapshotting
// ============================================================================
struct Rb26AtomicPointers {
    std::atomic<float>* inputTrimDb      { nullptr };
    std::atomic<float>* preDelayMs       { nullptr };
    std::atomic<float>* dryWetMix        { nullptr };
    std::atomic<float>* earlyLateMix     { nullptr };
    std::atomic<float>* lowCrossoverHz   { nullptr };
    std::atomic<float>* bassRt60Mult     { nullptr };
    std::atomic<float>* punchDucking     { nullptr };
    std::atomic<float>* subMonoHz        { nullptr };
    std::atomic<float>* roomSize         { nullptr };
    std::atomic<float>* decayRt60Sec     { nullptr };
    std::atomic<float>* highDampingHz    { nullptr };
    std::atomic<float>* diffusionDensity { nullptr };
    std::atomic<float>* freezeHold       { nullptr };
    std::atomic<float>* shimmerSend      { nullptr };
    std::atomic<float>* dimmerSend       { nullptr };
    std::atomic<float>* shimmerInterval  { nullptr };
    std::atomic<float>* dimmerInterval   { nullptr };
    std::atomic<float>* pitchBlend       { nullptr };
    std::atomic<float>* pitchFeedback    { nullptr };
    std::atomic<float>* pitchDelayMs     { nullptr };
    std::atomic<float>* tailModRateHz    { nullptr };
    std::atomic<float>* tailModDepthMs   { nullptr };
    std::atomic<float>* tailBloomMs      { nullptr };
    std::atomic<float>* stereoWidth      { nullptr };
    std::atomic<float>* outputTrimDb     { nullptr };
    std::atomic<float>* limiterEnable    { nullptr };

    void initialize(juce::AudioProcessorValueTreeState& apvts) noexcept {
        inputTrimDb       = apvts.getRawParameterValue(ParamIDs::inputTrimDb.getParamID());
        preDelayMs        = apvts.getRawParameterValue(ParamIDs::preDelayMs.getParamID());
        dryWetMix         = apvts.getRawParameterValue(ParamIDs::dryWetMix.getParamID());
        earlyLateMix      = apvts.getRawParameterValue(ParamIDs::earlyLateMix.getParamID());
        lowCrossoverHz    = apvts.getRawParameterValue(ParamIDs::lowCrossoverHz.getParamID());
        bassRt60Mult      = apvts.getRawParameterValue(ParamIDs::bassRt60Mult.getParamID());
        punchDucking      = apvts.getRawParameterValue(ParamIDs::punchDucking.getParamID());
        subMonoHz         = apvts.getRawParameterValue(ParamIDs::subMonoHz.getParamID());
        roomSize          = apvts.getRawParameterValue(ParamIDs::roomSize.getParamID());
        decayRt60Sec      = apvts.getRawParameterValue(ParamIDs::decayRt60Sec.getParamID());
        highDampingHz     = apvts.getRawParameterValue(ParamIDs::highDampingHz.getParamID());
        diffusionDensity  = apvts.getRawParameterValue(ParamIDs::diffusionDensity.getParamID());
        freezeHold        = apvts.getRawParameterValue(ParamIDs::freezeHold.getParamID());
        shimmerSend       = apvts.getRawParameterValue(ParamIDs::shimmerSend.getParamID());
        dimmerSend        = apvts.getRawParameterValue(ParamIDs::dimmerSend.getParamID());
        shimmerInterval   = apvts.getRawParameterValue(ParamIDs::shimmerInterval.getParamID());
        dimmerInterval    = apvts.getRawParameterValue(ParamIDs::dimmerInterval.getParamID());
        pitchBlend        = apvts.getRawParameterValue(ParamIDs::pitchBlend.getParamID());
        pitchFeedback     = apvts.getRawParameterValue(ParamIDs::pitchFeedback.getParamID());
        pitchDelayMs      = apvts.getRawParameterValue(ParamIDs::pitchDelayMs.getParamID());
        tailModRateHz     = apvts.getRawParameterValue(ParamIDs::tailModRateHz.getParamID());
        tailModDepthMs    = apvts.getRawParameterValue(ParamIDs::tailModDepthMs.getParamID());
        tailBloomMs       = apvts.getRawParameterValue(ParamIDs::tailBloomMs.getParamID());
        stereoWidth       = apvts.getRawParameterValue(ParamIDs::stereoWidth.getParamID());
        outputTrimDb      = apvts.getRawParameterValue(ParamIDs::outputTrimDb.getParamID());
        limiterEnable     = apvts.getRawParameterValue(ParamIDs::limiterEnable.getParamID());
    }

    [[nodiscard]] Rb26ParameterSnapshot loadSnapshot() const noexcept {
        Rb26ParameterSnapshot s;
        if (inputTrimDb)       s.inputTrimDb       = inputTrimDb->load(std::memory_order_relaxed);
        if (preDelayMs)        s.preDelayMs        = preDelayMs->load(std::memory_order_relaxed);
        if (dryWetMix)         s.dryWetMix         = dryWetMix->load(std::memory_order_relaxed);
        if (earlyLateMix)      s.earlyLateMix      = earlyLateMix->load(std::memory_order_relaxed);
        if (lowCrossoverHz)    s.lowCrossoverHz    = lowCrossoverHz->load(std::memory_order_relaxed);
        if (bassRt60Mult)      s.bassRt60Mult      = bassRt60Mult->load(std::memory_order_relaxed);
        if (punchDucking)      s.punchDucking      = punchDucking->load(std::memory_order_relaxed);
        if (subMonoHz)         s.subMonoHz         = subMonoHz->load(std::memory_order_relaxed);
        if (roomSize)          s.roomSize          = roomSize->load(std::memory_order_relaxed);
        if (decayRt60Sec)      s.decayRt60Sec      = decayRt60Sec->load(std::memory_order_relaxed);
        if (highDampingHz)     s.highDampingHz     = highDampingHz->load(std::memory_order_relaxed);
        if (diffusionDensity)  s.diffusionDensity  = diffusionDensity->load(std::memory_order_relaxed);
        if (freezeHold)        s.freezeHold        = (freezeHold->load(std::memory_order_relaxed) > 0.5f);
        if (shimmerSend)       s.shimmerSend       = shimmerSend->load(std::memory_order_relaxed);
        if (dimmerSend)        s.dimmerSend        = dimmerSend->load(std::memory_order_relaxed);
        if (shimmerInterval) {
            const float val = shimmerInterval->load(std::memory_order_relaxed);
            const int idx = static_cast<int>(std::round(val));
            s.shimmerInterval = (idx >= 0 && idx <= 2) ? shimmerIntervalFromIndex(idx) : static_cast<int>(val);
        }
        if (dimmerInterval) {
            const float val = dimmerInterval->load(std::memory_order_relaxed);
            const int idx = static_cast<int>(std::round(val));
            s.dimmerInterval = (idx >= 0 && idx <= 2) ? dimmerIntervalFromIndex(idx) : static_cast<int>(val);
        }
        if (pitchBlend)        s.pitchBlend        = pitchBlend->load(std::memory_order_relaxed);
        if (pitchFeedback)     s.pitchFeedback     = pitchFeedback->load(std::memory_order_relaxed);
        if (pitchDelayMs)      s.pitchDelayMs      = pitchDelayMs->load(std::memory_order_relaxed);
        if (tailModRateHz)     s.tailModRateHz     = tailModRateHz->load(std::memory_order_relaxed);
        if (tailModDepthMs)    s.tailModDepthMs    = tailModDepthMs->load(std::memory_order_relaxed);
        if (tailBloomMs)       s.tailBloomMs       = tailBloomMs->load(std::memory_order_relaxed);
        if (stereoWidth)       s.stereoWidth       = stereoWidth->load(std::memory_order_relaxed);
        if (outputTrimDb)      s.outputTrimDb      = outputTrimDb->load(std::memory_order_relaxed);
        if (limiterEnable)     s.limiterEnable     = (limiterEnable->load(std::memory_order_relaxed) > 0.5f);
        return s;
    }
};

// ============================================================================
// APVTS ParameterLayout Factory
// ============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 0. Input Trim (-18.0 - +18.0 dB, default 0.0 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::inputTrimDb,
        "Input Trim",
        juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // 1. Pre-Delay (0.0 - 500.0 ms, default 24.0 ms, skew 0.5 for fine low ms resolution)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::preDelayMs,
        "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 500.0f, 0.1f, 0.5f),
        24.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // 2. Dry/Wet Mix (0.0 - 1.0, default 0.40)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::dryWetMix,
        "Dry / Wet Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.40f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 3. Early/Late Mix (0.0 - 1.0, default 0.50)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::earlyLateMix,
        "Early / Late Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.50f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 4. Low Crossover Frequency (60.0 - 400.0 Hz, default 180.0 Hz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lowCrossoverHz,
        "Low Crossover Freq",
        juce::NormalisableRange<float>(60.0f, 400.0f, 0.1f, 0.5f),
        180.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // 5. Bass RT60 Multiplier (0.2 - 4.0x, default 1.0x, skew 0.6)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::bassRt60Mult,
        "Bass RT60 Multiplier",
        juce::NormalisableRange<float>(0.2f, 4.0f, 0.01f, 0.6f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("x")));

    // 6. Punch Ducking (0.0 - 1.0, default 0.65)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::punchDucking,
        "Punch Ducking",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.65f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 7. Sub Mono Maker Frequency (20.0 - 250.0 Hz, default 120.0 Hz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::subMonoHz,
        "Sub Mono Freq",
        juce::NormalisableRange<float>(20.0f, 250.0f, 0.1f, 0.5f),
        120.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // 8. Room Size (0.1 - 2.0, default 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::roomSize,
        "Room Size",
        juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f, 1.0f),
        1.0f));

    // 9. Decay Time RT60 (0.2 - 30.0 s, default 6.5 s, logarithmic skew 0.35)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::decayRt60Sec,
        "Decay Time (RT60)",
        juce::NormalisableRange<float>(0.2f, 30.0f, 0.01f, 0.35f),
        6.5f,
        juce::AudioParameterFloatAttributes().withLabel("s")));

    // 10. High Damping Frequency (1000.0 - 20000.0 Hz, default 7500.0 Hz, log skew 0.35)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::highDampingHz,
        "High Damping Freq",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.35f),
        7500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // 11. Diffusion Density (0.0 - 1.0, default 0.75)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::diffusionDensity,
        "Diffusion Density",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.75f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 12. Freeze Hold (bool, default false)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::freezeHold,
        "Freeze Hold",
        false));

    // 13. Shimmer Send (0.0 - 1.0, default 0.40)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::shimmerSend,
        "Shimmer Send",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.40f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 14. Dimmer Send (0.0 - 1.0, default 0.35)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::dimmerSend,
        "Dimmer Send",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f),
        0.35f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 15. Shimmer Pitch Interval (+7, +12, +24 semitones, default +12)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::shimmerInterval,
        "Shimmer Pitch Interval",
        getShimmerIntervalChoices(),
        1)); // index 1 = +12 st

    // 16. Dimmer Pitch Interval (-2, -7, -12 semitones, default -12)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::dimmerInterval,
        "Dimmer Pitch Interval",
        getDimmerIntervalChoices(),
        2)); // index 2 = -12 st

    // 17. Pitch Blend (-1.0 to +1.0, default 0.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::pitchBlend,
        "Pitch Blend (Dimmer / Shimmer)",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f, 1.0f),
        0.0f));

    // 18. Pitch Feedback (0.0 - 0.95, default 0.45)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::pitchFeedback,
        "Pitch Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f, 1.0f),
        0.45f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 19. Pitch Delay (20.0 - 500.0 ms, default 150.0 ms, skew 0.5)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::pitchDelayMs,
        "Pitch Delay",
        juce::NormalisableRange<float>(20.0f, 500.0f, 0.1f, 0.5f),
        150.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // 20. Tail Mod Rate (0.05 - 5.0 Hz, default 0.65 Hz, skew 0.5)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::tailModRateHz,
        "Tail Mod Rate",
        juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.5f),
        0.65f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // 20. Tail Mod Depth (0.0 - 5.0 ms, default 2.25 ms)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::tailModDepthMs,
        "Tail Mod Depth",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f, 1.0f),
        2.25f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // 21. Tail Bloom Attack (20.0 - 300.0 ms, default 85.0 ms, skew 0.6)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::tailBloomMs,
        "Tail Bloom Attack",
        juce::NormalisableRange<float>(20.0f, 300.0f, 0.1f, 0.6f),
        85.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // 22. Stereo Width (0.0 - 2.0, default 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::stereoWidth,
        "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f, 1.0f),
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 23. Output Trim (-24.0 - +12.0 dB, default 0.0 dB)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::outputTrimDb,
        "Output Trim",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.0f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // 24. Limiter Enable (bool, default true)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::limiterEnable,
        "Master Limiter",
        true));

    return { params.begin(), params.end() };
}

} // namespace rb26
