#pragma once

#include "DspMath.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstddef>

namespace rb26 {

// ============================================================================
// Enumerations & Type Definitions
// ============================================================================

enum class StimulusType : int {
    ChimeTine = 0,
    DiracImpulse = 1,
    PinkBurst = 2,
    HammerThud = 3
};

enum class ExciterRouting : int {
    ReverbOnly = 0,
    ReverbAndDirect = 1
};

enum class StrumSpeed : int {
    Slow = 0,
    Med = 1,
    Fast = 2,
    Instant = 3
};

enum class ModalScaleType : int {
    BuddPentatonic = 0,
    LydianDream = 1,
    DorianMystic = 2,
    YoshimuraAmbient = 3,
    AeolianMidnight = 4,
    BuddHexatonic = 5,
    WholeTone = 6,
    AvalonSpirited = 7
};

enum class ChordVoicingType : int {
    PavilionSus = 0,
    PlateauxMaj9 = 1,
    DeepDroneFifth = 2,
    Ethereal11th = 3,
    LydianCascade = 4,
    SolarBeating = 5,
    AvalonMaj9 = 6,
    SummersDay = 7,
    SpiritedSus = 8,
    Nostalgia11th = 9,
    BladeRunner = 10,
    TearsInRain = 11
};

// ============================================================================
// Model Constants & Lookup Tables
// ============================================================================

inline constexpr size_t kNumChimeModes = 4;
// Harold Budd felt piano acoustic modeling: fundamental, micro-detuned sympathetic string pair, octave, 3rd harmonic
inline constexpr std::array<float, kNumChimeModes> kChimeModeRatios = {
    1.0000f, 1.0012f, 2.0000f, 3.0000f
};
inline constexpr std::array<float, kNumChimeModes> kChimeModeDecayFactors = {
    1.000f, 1.200f, 2.500f, 4.000f
};

// 8 Modal Scales matching AS-42
struct ModalScaleDefinition {
    const char* id;
    const char* name;
    int numIntervals;
    std::array<int, 12> intervals;
};

inline constexpr size_t kNumModalScales = 8;
inline constexpr std::array<ModalScaleDefinition, kNumModalScales> kModalScales = {{
    { "BUDD_PENTATONIC",   "Budd Felt Pentatonic",    5, { 0, 2, 4, 7, 9 } },
    { "LYDIAN_DREAM",      "Lydian Ambient",          7, { 0, 2, 4, 6, 7, 9, 11 } },
    { "DORIAN_MYSTIC",     "Dorian Mystic",           7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "YOSHIMURA_AMBIENT", "Kankyo Ongaku",           5, { 0, 2, 5, 7, 9 } },
    { "AEOLIAN_MIDNIGHT",  "Aeolian Midnight",        7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "BUDD_HEXATONIC",    "Budd Hexatonic",          6, { 0, 2, 4, 5, 7, 9 } },
    { "WHOLE_TONE",        "Weightless Whole Tone",   6, { 0, 2, 4, 6, 8, 10 } },
    { "AVALON_SPIRITED",   "Avalon / Spirited Modal", 7, { 0, 2, 4, 5, 7, 9, 11 } }
}};

// 12 Signature Chord Voicings matching AS-42
struct ChordVoicingDefinition {
    const char* id;
    const char* name;
    int numNotes;
    std::array<float, 8> intervals;
};

inline constexpr size_t kNumChordVoicings = 12;
inline constexpr std::array<ChordVoicingDefinition, kNumChordVoicings> kChordVoicings = {{
    { "PAVILION_SUS",      "Pavilion Suspended",    4, { 0.0f, 7.0f, 14.0f, 16.0f } },
    { "PLATEAUX_MAJ9",     "Plateaux Major 9",      5, { 0.0f, 4.0f, 7.0f, 11.0f, 14.0f } },
    { "DEEP_DRONE_FIFTH",  "Deep Drone Fifth",      5, { 0.0f, 7.0f, 12.0f, 19.0f, 24.0f } },
    { "ETHEREAL_11TH",     "Ethereal 11th",         6, { 0.0f, 7.0f, 11.0f, 14.0f, 17.0f, 24.0f } },
    { "LYDIAN_CASCADE",    "Lydian Cascade",        6, { 0.0f, 4.0f, 6.0f, 7.0f, 11.0f, 14.0f } },
    { "SOLAR_BEATING",     "Solar Beating Drone",   5, { 0.0f, 0.08f, 7.0f, 7.06f, 12.0f } },
    { "AVALON_MAJ9",       "Avalon Maj9",           5, { 0.0f, 7.0f, 11.0f, 14.0f, 16.0f } },
    { "SUMMERS_DAY",       "Summer's Day",          5, { 0.0f, 7.0f, 14.0f, 16.0f, 19.0f } },
    { "SPIRITED_SUS",      "Spirited Sus",          5, { 0.0f, 7.0f, 12.0f, 14.0f, 17.0f } },
    { "NOSTALGIA_11TH",    "Nostalgia 11th",        6, { 0.0f, 7.0f, 10.0f, 14.0f, 15.0f, 17.0f } },
    { "BLADE_RUNNER",      "Blade Runner",          6, { 0.0f, 7.0f, 10.0f, 14.0f, 17.0f, 20.0f } },
    { "TEARS_IN_RAIN",     "Tears in Rain",         6, { 0.0f, 7.0f, 11.0f, 14.0f, 18.0f, 21.0f } }
}};

// 4 Strum Speeds matching AS-42
struct StrumSpeedDefinition {
    const char* id;
    const char* name;
    float rateMs;
    float jitterMs;
};

inline constexpr size_t kNumStrumSpeeds = 4;
inline constexpr std::array<StrumSpeedDefinition, kNumStrumSpeeds> kStrumSpeeds = {{
    { "slow",    "SLOW",    120.0f, 10.0f },
    { "med",     "MED",      50.0f,  6.0f },
    { "fast",    "FAST",     20.0f,  4.0f },
    { "instant", "INSTANT",   0.0f,  0.0f }
}};

// ============================================================================
// Helper Functions (Velocity & Modal Quantization)
// ============================================================================

// Velocity sensitivity: velocity = 0.35 + 0.50 * relY (matching AS-42 keyboard.js)
[[nodiscard]] inline float calculateVelocityFromRelY(float relY) noexcept {
    return 0.35f + 0.50f * std::clamp(relY, 0.0f, 1.0f);
}

// Convert MIDI note number to frequency (Hz)
[[nodiscard]] inline float midiToFrequency(float midiNote, float a4 = 440.0f) noexcept {
    return a4 * std::pow(2.0f, (midiNote - 69.0f) * (1.0f / 12.0f));
}

// Modal scale quantizer matching AS-42 quantizeToScale in scales.js
[[nodiscard]] inline float quantizeMidiToScale(float rawMidi, int rootPitchClass, const int* intervals, int numIntervals) noexcept {
    if (numIntervals <= 0 || intervals == nullptr) return std::round(rawMidi);
    const int normRoot = ((rootPitchClass % 12) + 12) % 12;
    const int baseOctave = static_cast<int>(std::floor((rawMidi - static_cast<float>(normRoot)) / 12.0f));
    const float relSemitone = (rawMidi - static_cast<float>(normRoot)) - (static_cast<float>(baseOctave) * 12.0f);

    float minDiff = 1e9f;
    int bestInterval = intervals[0];

    for (int i = 0; i < numIntervals; ++i) {
        const float diff = std::abs(relSemitone - static_cast<float>(intervals[i]));
        if (diff < minDiff) {
            minDiff = diff;
            bestInterval = intervals[i];
        }
    }
    // Check wrap to upper octave (e.g. interval 0 in next octave)
    const float wrapUp = std::abs(relSemitone - static_cast<float>(intervals[0] + 12));
    if (wrapUp < minDiff) {
        minDiff = wrapUp;
        bestInterval = intervals[0] + 12;
    }
    // Check wrap to lower octave
    const float wrapDown = std::abs(relSemitone - static_cast<float>(intervals[numIntervals - 1] - 12));
    if (wrapDown < minDiff) {
        minDiff = wrapDown;
        bestInterval = intervals[numIntervals - 1] - 12;
    }

    return static_cast<float>(normRoot + baseOctave * 12 + bestInterval);
}

[[nodiscard]] inline float quantizeMidiToScaleIndex(float rawMidi, int rootPitchClass, int scaleIndex) noexcept {
    const int idx = std::clamp(scaleIndex, 0, static_cast<int>(kNumModalScales - 1));
    return quantizeMidiToScale(rawMidi, rootPitchClass, kModalScales[idx].intervals.data(), kModalScales[idx].numIntervals);
}

// ============================================================================
// Real-Time Safe PRNG (XorShift64* - Bit-Exact, Zero Heap Allocs)
// ============================================================================
class FastPrng {
public:
    explicit FastPrng(uint64_t seed = 0x853c49e6748fea9bULL) noexcept : mState(seed != 0 ? seed : 1ULL) {}

    void setSeed(uint64_t seed) noexcept {
        mState = (seed != 0 ? seed : 1ULL);
    }

    [[nodiscard]] uint32_t nextU32() noexcept {
        mState ^= mState >> 12;
        mState ^= mState << 25;
        mState ^= mState >> 27;
        return static_cast<uint32_t>((mState * 0x2545F4914F6CDD1DULL) >> 32);
    }

    [[nodiscard]] float nextFloat01() noexcept {
        return static_cast<float>(nextU32()) * (1.0f / 4294967296.0f);
    }

    [[nodiscard]] float nextFloatRange(float minVal, float maxVal) noexcept {
        return minVal + (maxVal - minVal) * nextFloat01();
    }

    [[nodiscard]] float nextGaussian01() noexcept {
        // Irwin-Hall approx: mean of 3 uniform distributions
        return (nextFloat01() + nextFloat01() + nextFloat01()) * (1.0f / 3.0f);
    }

private:
    uint64_t mState { 0x853c49e6748fea9bULL };
};

// ============================================================================
// ChimeVoice: Physical Inharmonic Modal Chime & Tines Voice
// fk = f0 * [1.000, 2.756, 5.404, 8.933]
// ============================================================================
class ChimeVoice {
public:
    ChimeVoice() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void trigger(float midiNote, float velocity, float durationSec = 3.5f) noexcept;
    void release() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return mActive; }
    [[nodiscard]] uint32_t getAge() const noexcept { return mAge; }
    [[nodiscard]] float getMidiNote() const noexcept { return mMidiNote; }

    void processSample(float& outL, float& outR) noexcept;

private:
    float mSampleRate { 48000.0f };
    bool mActive { false };
    uint32_t mAge { 0 };
    float mMidiNote { 60.0f };
    float mVelocity { 0.70f };

    std::array<float, kNumChimeModes> mPhase { 0.0f };
    std::array<float, kNumChimeModes> mPhaseInc { 0.0f };
    std::array<float, kNumChimeModes> mEnv { 0.0f };
    std::array<float, kNumChimeModes> mDecayCoeff { 0.0f };
    std::array<float, kNumChimeModes> mAmp { 0.0f };

    float mNoiseEnv { 0.0f };
    float mNoiseDecay { 0.0f };
    FastPrng mPrng { 0x12345678ULL };
};

// ============================================================================
// LaboratoryImpulseGenerator: Precision Acoustic Calibration Signals
// - 1-Sample Dirac Delta Pulse
// - Broadband Paul Kellet 3-Pole Pink Noise Burst with Tukey Cosine Window
// - Acoustic Soundboard Hammer Thud (78 Hz body resonance + Hertzian contact click)
// ============================================================================
class LaboratoryImpulseGenerator {
public:
    LaboratoryImpulseGenerator() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void triggerDirac(float polarity = 1.0f) noexcept;
    void triggerPinkBurst(float durationMs = 40.0f) noexcept;
    void triggerHammerThud(float hardness = 0.7f) noexcept;

    [[nodiscard]] bool isBusy() const noexcept {
        return (mDiracRemaining > 0) || (mBurstRemaining > 0) || (mHammerRemaining > 0);
    }

    void processSample(float& outL, float& outR) noexcept;

private:
    float mSampleRate { 48000.0f };

    // 1. Dirac
    int mDiracRemaining { 0 };
    float mDiracPolarity { 1.0f };

    // 2. Pink Noise Burst
    int mBurstRemaining { 0 };
    int mBurstTotal { 0 };
    float mB0 { 0.0f }, mB1 { 0.0f }, mB2 { 0.0f };
    FastPrng mPinkPrng { 0x87654321ULL };

    // 3. Hammer Thud
    int mHammerRemaining { 0 };
    int mHammerTotal { 0 };
    float mHammerPhase { 0.0f };
    float mHammerFreqInc { 0.0f };
    float mHammerEnv { 0.0f };
    float mHammerDecay { 0.0f };
};

// ============================================================================
// PoissonClock: Continuous Stationary Ambient Event Generator
// tau = -ln(1 - U) / lambda with Harold Budd Step/Leap Markov & Irwin-Hall Velocity
// ============================================================================
class PoissonClock {
public:
    PoissonClock() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Step clock by 1 sample. Returns true if an ambient event should trigger.
    bool tick(float epm, float humanize, float& outMidi, float& outVelocity,
              int rootPitchClass, int scaleIndex,
              const int* customIntervals = nullptr, int numCustomIntervals = 0) noexcept;

private:
    float mSampleRate { 48000.0f };
    int mSamplesUntilNext { 48000 };
    float mLastMidiNote { 60.0f };
    FastPrng mPrng { 0x54321987ULL };
};

// ============================================================================
// Lock-Free SPSC FIFO Trigger Event & Scheduled Note Structures
// ============================================================================

struct ExciterTriggerEvent {
    StimulusType type { StimulusType::ChimeTine };
    float midiNote { 60.0f };
    float velocity { 0.70f };
    float durationSec { 3.5f };
    int chordIndex { -1 };                      // -1 for single note, 0..11 for chord macro
    StrumSpeed strumSpeed { StrumSpeed::Med };
    float param1 { 0.0f };                      // Polarity / Burst duration ms / Hammer hardness
    int sampleOffset { 0 };                     // Intra-block sample offset
};

struct ScheduledNote {
    bool active { false };
    int delaySamples { 0 };
    float midiNote { 60.0f };
    float velocity { 0.70f };
    float durationSec { 3.5f };
};

// ============================================================================
// Complete Onboard Acoustic Stimulus & Exciter Engine
// Fixed 16-Voice Pool, 0 Heap Allocations, Lock-Free SPSC Queue
// ============================================================================

struct AcousticExciterParameters {
    bool enable { true };
    float levelDb { 0.0f };                     // -48.0 dB to +6.0 dB
    ExciterRouting routing { ExciterRouting::ReverbAndDirect };

    // Scale and harmony
    int scaleIndex { 0 };                       // 0 to 7 (BUDD_PENTATONIC ... AVALON_SPIRITED)
    int rootPitchClass { 0 };                   // 0 to 11 (C, C#, ..., B)
    StrumSpeed chordSpeed { StrumSpeed::Med };

    // Laboratory impulse parameters
    StimulusType labImpulseType { StimulusType::DiracImpulse };
    float labBurstDurationMs { 40.0f };         // 10 to 200 ms
    float labHammerHardness { 0.7f };           // 0.0 to 1.0
    float labDiracPolarity { 1.0f };            // +1.0 or -1.0

    // Poisson generative clock parameters
    bool poissonEnable { false };
    float poissonEpm { 12.0f };                 // 4.0 to 60.0 events per minute
    float poissonHumanize { 0.50f };            // 0.0 to 1.0
};

class AcousticExciterEngine {
public:
    static constexpr size_t kMaxVoices = 16;
    static constexpr size_t kFifoCapacity = 128;
    static constexpr size_t kFifoMask = kFifoCapacity - 1;
    static constexpr size_t kMaxScheduledNotes = 64;

    AcousticExciterEngine() noexcept;
    ~AcousticExciterEngine() noexcept = default;

    // Hard real-time lifecycle
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Parameter configuration
    void setParameters(const AcousticExciterParameters& params) noexcept;
    [[nodiscard]] const AcousticExciterParameters& getParameters() const noexcept { return mParams; }
    void setEnable(bool enable) noexcept { mParams.enable = enable; }
    void setLevelDb(float db) noexcept;
    void setRouting(ExciterRouting routing) noexcept { mParams.routing = routing; }
    void setScale(int scaleIndex) noexcept { mParams.scaleIndex = std::clamp(scaleIndex, 0, static_cast<int>(kNumModalScales - 1)); }
    void setRootPitchClass(int root) noexcept { mParams.rootPitchClass = ((root % 12) + 12) % 12; }
    void setChordSpeed(StrumSpeed speed) noexcept { mParams.chordSpeed = speed; }
    void setPoissonEnable(bool enable) noexcept { mParams.poissonEnable = enable; }
    void setPoissonEpm(float epm) noexcept { mParams.poissonEpm = std::clamp(epm, 4.0f, 60.0f); }
    void setPoissonHumanize(float humanize) noexcept { mParams.poissonHumanize = std::clamp(humanize, 0.0f, 1.0f); }

    // Lock-Free SPSC FIFO interface (thread-safe, callable from UI thread or message thread)
    bool postTriggerEvent(const ExciterTriggerEvent& evt) noexcept;
    bool triggerNoteAsync(float midiNote, float velocity, float durationSec = 3.5f) noexcept;
    bool triggerChordAsync(int chordIndex, float rootMidi, float velocity, StrumSpeed speed = StrumSpeed::Med) noexcept;
    bool triggerDiracAsync(float polarity = 1.0f) noexcept;
    bool triggerPinkBurstAsync(float durationMs = 40.0f) noexcept;
    bool triggerHammerThudAsync(float hardness = 0.7f) noexcept;

    // Audio thread direct voice control (e.g. from incoming MIDI Note-On / Note-Off)
    void triggerVoice(float midiNote, float velocity, float durationSec = 3.5f) noexcept;
    void releaseVoice(float midiNote) noexcept;
    void releaseAllVoices() noexcept;
    void triggerChord(int chordIndex, float rootMidi, float velocity, StrumSpeed speed = StrumSpeed::Med) noexcept;
    void triggerDirac(float polarity = 1.0f) noexcept;
    void triggerPinkBurst(float durationMs = 40.0f) noexcept;
    void triggerHammerThud(float hardness = 0.7f) noexcept;

    // Process methods:
    // 1. Standalone buffer generation
    void process(float* outL, float* outR, int numSamples) noexcept;

    // 2. Direct bus + Reverb bus injection (matching PROJECT.md and survey_exciter.md)
    void process(float* const* directBus, float* const* reverbBus, int numSamples) noexcept;

    // 3. Explicit parameter override process
    void process(float* const* directBus, float* const* reverbBus, int numSamples,
                 bool exciterEnable, float exciterLevelDb, ExciterRouting routing,
                 bool poissonEnable, float poissonEpm, float poissonHumanize,
                 int rootPitchClass, const int* scaleIntervals, int numScaleIntervals) noexcept;

    // Status queries
    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] bool isPoissonActive() const noexcept { return mParams.poissonEnable; }

private:
    double mSampleRate { 48000.0 };
    AcousticExciterParameters mParams;

    // 16-Voice Chime Pool
    std::array<ChimeVoice, kMaxVoices> mVoices;

    // Precision Laboratory Generator
    LaboratoryImpulseGenerator mLabGen;

    // Poisson Generative Engine
    PoissonClock mPoissonClock;

    // Chord Strumming Queue
    std::array<ScheduledNote, kMaxScheduledNotes> mScheduledNotes {};
    FastPrng mStrumPrng { 0x99887766ULL };

    // SPSC Lock-Free Event FIFO
    std::array<ExciterTriggerEvent, kFifoCapacity> mEventFifo {};
    std::atomic<size_t> mFifoWritePos { 0 };
    std::atomic<size_t> mFifoReadPos { 0 };

    // Level smoothing
    OnePoleSmoother mLevelSmoother;

    void drainTriggerQueue() noexcept;
    void handleTriggerEvent(const ExciterTriggerEvent& evt) noexcept;
};

} // namespace rb26
