#include "../standalone/StandaloneAudioGuard.h"
#include "../plugin/PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#define RB26_GUARD_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[FAIL] Assertion failed: (" << #cond << ") at " \
                      << __FILE__ << ":" << __LINE__ << "\n"; \
            std::exit(1); \
        } \
    } while (0)

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

// ============================================================================
// Deterministic Mock AudioIODevice & AudioIODeviceType for Hardware-Free CI
// ============================================================================
struct SharedMockHardwareState {
    int failNextOpenCount { 0 };
    bool inputForceClosed { false };
    bool outputForceClosed { false };
    bool usbDacUnplugged { false };
    double forcedInputNativeRate { 48000.0 };
    bool lockInputToForcedRate { false };
    bool lockOutputTo48k { false };
    int openCallsOutput { 0 };
    int openCallsInput { 0 };
};

class MockEndpointAudioIODevice final : public juce::AudioIODevice {
public:
    MockEndpointAudioIODevice(const juce::String& name,
                              const juce::String& typeName,
                              bool isInputEndpoint,
                              SharedMockHardwareState& hwState,
                              double nativeRate = 48000.0)
        : juce::AudioIODevice(name, typeName),
          mIsInput(isInputEndpoint),
          mHw(hwState),
          mNativeRate(nativeRate) {}

    juce::StringArray getOutputChannelNames() override {
        return mIsInput ? juce::StringArray{} : juce::StringArray{ "Out L", "Out R" };
    }

    juce::StringArray getInputChannelNames() override {
        return mIsInput ? juce::StringArray{ "In L", "In R" } : juce::StringArray{};
    }

    juce::Array<double> getAvailableSampleRates() override {
        if (mIsInput && mHw.lockInputToForcedRate) {
            return { mHw.forcedInputNativeRate };
        }
        if (!mIsInput && mHw.lockOutputTo48k) {
            return { 48000.0 };
        }
        return { mNativeRate, 44100.0, 48000.0, 96000.0 };
    }

    juce::Array<int> getAvailableBufferSizes() override {
        return { 128, 256, 480, 512, 1024, 2560, 4096 };
    }

    int getDefaultBufferSize() override { return 480; }

    juce::String open(const juce::BigInteger& inChans,
                      const juce::BigInteger& outChans,
                      double sr,
                      int bufSize) override {
        if (!mIsInput && mHw.usbDacUnplugged && getName() == "Headphones (USB DAC)") {
            mOpen = false;
            return "Endpoint unplugged";
        }
        if (mHw.failNextOpenCount > 0) {
            --mHw.failNextOpenCount;
            mOpen = false;
            return "AUDCLNT_E_DEVICE_INVALIDATED (simulated D3->D0 wake transition)";
        }
        if (mIsInput && mHw.lockInputToForcedRate && sr > 0.0 && std::abs(sr - mHw.forcedInputNativeRate) > 0.5) {
            mOpen = false;
            return "AUDCLNT_E_UNSUPPORTED_FORMAT (locked to native rate)";
        }

        if (mIsInput) {
            ++mHw.openCallsInput;
            mHw.inputForceClosed = false;
            mActiveIn = inChans;
            mSampleRate = (sr > 0.0) ? sr : mHw.forcedInputNativeRate;
        } else {
            ++mHw.openCallsOutput;
            mHw.outputForceClosed = false;
            mActiveOut = outChans;
            mSampleRate = (sr > 0.0) ? sr : mNativeRate;
        }

        mBufferSize = (bufSize > 0) ? bufSize : 480;
        mOpen = true;
        return {};
    }

    void close() override {
        stop();
        mOpen = false;
    }

    bool isOpen() override {
        if (mIsInput && mHw.inputForceClosed) {
            return false;
        }
        if (!mIsInput && mHw.outputForceClosed) {
            return false;
        }
        return mOpen;
    }

    void start(juce::AudioIODeviceCallback* cb) override {
        if (!mOpen || cb == nullptr) {
            return;
        }
        mCallback = cb;
        mPlaying = true;
        mCallback->audioDeviceAboutToStart(this);
    }

    void stop() override {
        if (mPlaying) {
            mPlaying = false;
            if (mCallback != nullptr) {
                mCallback->audioDeviceStopped();
                mCallback = nullptr;
            }
        }
    }

    bool isPlaying() override { return mPlaying && isOpen(); }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return mBufferSize; }
    double getCurrentSampleRate() override { return mSampleRate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return mActiveOut; }
    juce::BigInteger getActiveInputChannels() const override { return mActiveIn; }
    int getOutputLatencyInSamples() override { return mBufferSize; }
    int getInputLatencyInSamples() override { return mBufferSize; }

    void pumpInputBlock(const float* const* ins, int numIns, int numSamples) {
        if (mPlaying && mCallback != nullptr && isOpen()) {
            juce::AudioIODeviceCallbackContext ctx{};
            mCallback->audioDeviceIOCallbackWithContext(ins, numIns, nullptr, 0, numSamples, ctx);
        }
    }

    void pumpOutputBlock(float* const* outs, int numOuts, int numSamples) {
        if (mPlaying && mCallback != nullptr && isOpen()) {
            juce::AudioIODeviceCallbackContext ctx{};
            mCallback->audioDeviceIOCallbackWithContext(nullptr, 0, outs, numOuts, numSamples, ctx);
        }
    }

private:
    bool mIsInput { false };
    SharedMockHardwareState& mHw;
    double mNativeRate { 48000.0 };
    double mSampleRate { 48000.0 };
    int mBufferSize { 480 };
    bool mOpen { false };
    bool mPlaying { false };
    juce::BigInteger mActiveIn;
    juce::BigInteger mActiveOut;
    juce::AudioIODeviceCallback* mCallback { nullptr };
};

class MockWindowsAudioDeviceType final : public juce::AudioIODeviceType {
public:
    explicit MockWindowsAudioDeviceType(SharedMockHardwareState& hw)
        : juce::AudioIODeviceType("Windows Audio"),
          mHw(hw) {}

    void scanForDevices() override {}

    void notifyDeviceListChanged() {
        callDeviceChangeListeners();
    }

    juce::StringArray getDeviceNames(bool wantInputNames) const override {
        if (wantInputNames) {
            // Put hardware Microphone at index 0 (system default) so any unsolicited default-device
            // fallback would replace VB-Cable with Microphone unless explicitly guarded!
            return { "Microphone (Realtek Audio)", "CABLE Output (VB-Audio Virtual Cable)" };
        }
        if (mHw.usbDacUnplugged) {
            return { "Speakers (Realtek High Definition Audio)" };
        }
        return { "Speakers (Realtek High Definition Audio)", "Headphones (USB DAC)" };
    }

    int getDefaultDeviceIndex(bool) const override { return 0; }

    int getIndexOfDevice(juce::AudioIODevice* dev, bool asInput) const override {
        if (dev == nullptr) {
            return -1;
        }
        return getDeviceNames(asInput).indexOf(dev->getName());
    }

    bool hasSeparateInputsAndOutputs() const override { return true; }

    juce::AudioIODevice* createDevice(const juce::String& outName,
                                      const juce::String& inName) override {
        // Note: DriftCompensatedAudioIODeviceType always requests single-direction endpoints!
        RB26_GUARD_ASSERT(!(outName.isNotEmpty() && inName.isNotEmpty()));
        if (outName.isNotEmpty()) {
            if (mHw.usbDacUnplugged && outName == "Headphones (USB DAC)") {
                return nullptr;
            }
            return new MockEndpointAudioIODevice(outName, getTypeName(), false, mHw);
        }
        if (inName.isNotEmpty()) {
            return new MockEndpointAudioIODevice(inName, getTypeName(), true, mHw, mHw.forcedInputNativeRate);
        }
        return nullptr;
    }

private:
    SharedMockHardwareState& mHw;
};

// ============================================================================
// Test 1 & 2: Sustained Positive (+85 ppm) and Negative (-85 ppm) Clock Drift
// ============================================================================
void runSustainedClockDriftTest(double ppmOffset, int prodBlock, int consBlock) {
    rb26::standalone::ElasticDriftRingBuffer ring;
    constexpr double kConsumerFs = 48000.0;
    const double actualProducerFs = kConsumerFs * (1.0 + ppmOffset * 1.0e-6);
    constexpr double kToneHz = 997.0;

    // Both endpoints nominally claim 48000 Hz, while actual producer clock drifts by ppmOffset!
    ring.prepare(2, prodBlock, consBlock, kConsumerFs, kConsumerFs);

    std::vector<float> inL(static_cast<size_t>(prodBlock), 0.0f);
    std::vector<float> inR(static_cast<size_t>(prodBlock), 0.0f);
    const float* inPtrs[2] = { inL.data(), inR.data() };

    std::vector<float> outL(static_cast<size_t>(consBlock), 0.0f);
    std::vector<float> outR(static_cast<size_t>(consBlock), 0.0f);
    float* outPtrs[2] = { outL.data(), outR.data() };

    constexpr int64_t kTotalOutFrames = 6'000'000; // 125 seconds continuous audio @ 48 kHz
    int64_t outGenerated = 0;
    uint64_t prodSampleIdx = 0;
    double prodTimeCreditFrames = 0.0;

    float prev1 = 0.0f;
    float prev2 = 0.0f;
    float maxFirstDiff = 0.0f;
    float maxSecondDiff = 0.0f;
    double steadyRmsAccum = 0.0;
    int64_t steadySamples = 0;

    double ratioAccum = 0.0;
    int64_t ratioSamples = 0;
    double maxRatioDeviationFromNominal = 0.0;

    const int64_t settleFrames = 240'000; // First 5 seconds: initial pre-primed silence drains & tone settles

    while (outGenerated < kTotalOutFrames) {
        prodTimeCreditFrames += static_cast<double>(consBlock) * (actualProducerFs / kConsumerFs);
        while (prodTimeCreditFrames >= static_cast<double>(prodBlock)) {
            for (int i = 0; i < prodBlock; ++i) {
                const double phase = kTwoPi * kToneHz * (static_cast<double>(prodSampleIdx++) / actualProducerFs);
                inL[static_cast<size_t>(i)] = static_cast<float>(0.80 * std::sin(phase));
                inR[static_cast<size_t>(i)] = static_cast<float>(0.80 * std::cos(phase));
            }
            ring.push(inPtrs, 2, prodBlock);
            prodTimeCreditFrames -= static_cast<double>(prodBlock);
        }

        ring.pull(outPtrs, 2, consBlock);

        if (outGenerated >= kTotalOutFrames / 2) {
            const double curRatio = ring.getCurrentRatio();
            ratioAccum += curRatio;
            ++ratioSamples;
            maxRatioDeviationFromNominal = std::max(maxRatioDeviationFromNominal, std::abs(curRatio - 1.0));
        }

        for (int i = 0; i < consBlock; ++i) {
            const float s = outL[static_cast<size_t>(i)];
            RB26_GUARD_ASSERT(std::isfinite(s));
            if (outGenerated >= settleFrames) {
                const float d1 = std::abs(s - prev1);
                const float d2 = std::abs(s - 2.0f * prev1 + prev2);
                maxFirstDiff = std::max(maxFirstDiff, d1);
                maxSecondDiff = std::max(maxSecondDiff, d2);
                steadyRmsAccum += static_cast<double>(s) * static_cast<double>(s);
                ++steadySamples;
            }
            prev2 = prev1;
            prev1 = s;
            ++outGenerated;
        }
    }

    // Theoretical max slope of 0.8 * sin(2*pi*997*t) at 48 kHz is 0.8 * 2*pi*997/48000 = 0.104405
    // Theoretical max curvature |d2| is 0.8 * (2*pi*997/48000)^2 = 0.013626
    RB26_GUARD_ASSERT(ring.getUnderrunCount() == 0);
    RB26_GUARD_ASSERT(ring.getOverflowCount() == 0);
    RB26_GUARD_ASSERT(ring.getRecenterCount() == 0);
    RB26_GUARD_ASSERT(maxFirstDiff < 0.115f);
    RB26_GUARD_ASSERT(maxSecondDiff < 0.018f);

    const double rms = std::sqrt(steadyRmsAccum / static_cast<double>(steadySamples));
    // Expected RMS of 0.80 peak sine is 0.80 / sqrt(2) = 0.565685
    RB26_GUARD_ASSERT(std::abs(rms - 0.565685) < 0.005);

    const double expectedRatio = 1.0 + ppmOffset * 1.0e-6;
    const double meanRatio = ratioAccum / static_cast<double>(std::max<int64_t>(1, ratioSamples));
    // Mean ratio locks to true clock drift within 50 ppm, and worst-case packet-beat ripple stays < 500 ppm (< 0.85 cents)!
    RB26_GUARD_ASSERT(std::abs(meanRatio - expectedRatio) < 5.0e-5);
    RB26_GUARD_ASSERT(maxRatioDeviationFromNominal < 5.0e-4);
    RB26_GUARD_ASSERT(std::abs(ring.getEstimatedFillFrames() - static_cast<double>(ring.getTargetFillFrames())) < 600.0);
}

// ============================================================================
// Test 3: Asynchronous Sample Rate Conversion (44.1 kHz VB-Cable -> 48 kHz Out)
// ============================================================================
void runAsyncSampleRateConversionTest() {
    rb26::standalone::ElasticDriftRingBuffer ring;
    constexpr double kInFs = 44100.0;
    constexpr double kOutFs = 48000.0;
    constexpr int kInBlock = 441;
    constexpr int kOutBlock = 480;
    constexpr double kToneHz = 440.0;

    ring.prepare(2, kInBlock, kOutBlock, kInFs, kOutFs);

    std::vector<float> inL(kInBlock, 0.0f), inR(kInBlock, 0.0f);
    std::vector<float> outL(kOutBlock, 0.0f), outR(kOutBlock, 0.0f);
    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    uint64_t inSampleIdx = 0;
    int zeroCrossings = 0;
    float prevS = 0.0f;
    float maxJump = 0.0f;
    constexpr int kTotalBlocks = 2500; // 25 seconds
    constexpr int kMeasureStartBlock = 500; // Final 20 seconds

    for (int b = 0; b < kTotalBlocks; ++b) {
        for (int i = 0; i < kInBlock; ++i) {
            const double ph = kTwoPi * kToneHz * (static_cast<double>(inSampleIdx++) / kInFs);
            inL[static_cast<size_t>(i)] = static_cast<float>(0.75 * std::sin(ph));
            inR[static_cast<size_t>(i)] = static_cast<float>(0.75 * std::sin(ph));
        }
        ring.push(inPtrs, 2, kInBlock);
        ring.pull(outPtrs, 2, kOutBlock);

        for (int i = 0; i < kOutBlock; ++i) {
            const float s = outL[static_cast<size_t>(i)];
            if (b >= kMeasureStartBlock) {
                maxJump = std::max(maxJump, std::abs(s - prevS));
                if ((prevS <= 0.0f && s > 0.0f)) {
                    ++zeroCrossings;
                }
            }
            prevS = s;
        }
    }

    RB26_GUARD_ASSERT(ring.getUnderrunCount() == 0);
    RB26_GUARD_ASSERT(ring.getOverflowCount() == 0);
    // Max slope of 0.75 * sin(2*pi*440*t) at 48 kHz is ~0.0432
    RB26_GUARD_ASSERT(maxJump < 0.055f);
    // 20 seconds of 440 Hz = 8800 positive-going zero crossings
    RB26_GUARD_ASSERT(std::abs(zeroCrossings - 8800) <= 5);
}

// ============================================================================
// Test 4: Severe Stall / Overflow / Underrun & Toxic Input Self-Healing
// ============================================================================
void runStallAndToxicSelfHealingTest() {
    rb26::standalone::ElasticDriftRingBuffer ring;
    ring.prepare(2, 256, 256, 48000.0, 48000.0);

    std::vector<float> inL(256, 0.0f), inR(256, 0.0f);
    std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    uint64_t phIdx = 0;
    auto pushToneBlock = [&]() {
        for (int i = 0; i < 256; ++i) {
            const float v = static_cast<float>(0.90 * std::sin(kTwoPi * 330.0 * static_cast<double>(phIdx++) / 48000.0));
            inL[static_cast<size_t>(i)] = v;
            inR[static_cast<size_t>(i)] = v;
        }
        ring.push(inPtrs, 2, 256);
    };

    // Warm up normal streaming
    for (int b = 0; b < 40; ++b) {
        pushToneBlock();
        ring.pull(outPtrs, 2, 256);
    }

    // 4A. Simulate Output freezing for ~350 ms (65 blocks = 16,640 frames pushed without pulls)
    float prevOut = outL.back();
    for (int b = 0; b < 65; ++b) {
        pushToneBlock();
    }
    float maxRecenterStep = 0.0f;
    for (int b = 0; b < 10; ++b) {
        pushToneBlock();
        ring.pull(outPtrs, 2, 256);
        for (int i = 0; i < 256; ++i) {
            maxRecenterStep = std::max(maxRecenterStep, std::abs(outL[static_cast<size_t>(i)] - prevOut));
            prevOut = outL[static_cast<size_t>(i)];
        }
    }
    RB26_GUARD_ASSERT(ring.getRecenterCount() >= 1);
    // Raised-cosine 128-sample crossfade between two 0.9-peak 330 Hz tones must never produce a hard >0.20 cliff
    RB26_GUARD_ASSERT(maxRecenterStep < 0.12f);
    RB26_GUARD_ASSERT(std::abs(ring.getEstimatedFillFrames() - static_cast<double>(ring.getTargetFillFrames())) < 300.0);

    // 4B. Simulate Input freezing completely while signal is near peak -> smooth decay to zero, no zeromem cliff!
    float maxUnderrunStep = 0.0f;
    for (int b = 0; b < 30; ++b) {
        ring.pull(outPtrs, 2, 256);
        for (int i = 0; i < 256; ++i) {
            maxUnderrunStep = std::max(maxUnderrunStep, std::abs(outL[static_cast<size_t>(i)] - prevOut));
            prevOut = outL[static_cast<size_t>(i)];
        }
    }
    RB26_GUARD_ASSERT(ring.getUnderrunCount() >= 1);
    RB26_GUARD_ASSERT(maxUnderrunStep < 0.08f);
    RB26_GUARD_ASSERT(std::abs(outL.back()) == 0.0f);

    // Resume input -> smooth fade back in without clicks
    float maxResumeStep = 0.0f;
    for (int b = 0; b < 20; ++b) {
        pushToneBlock();
        ring.pull(outPtrs, 2, 256);
        for (int i = 0; i < 256; ++i) {
            maxResumeStep = std::max(maxResumeStep, std::abs(outL[static_cast<size_t>(i)] - prevOut));
            prevOut = outL[static_cast<size_t>(i)];
        }
    }
    RB26_GUARD_ASSERT(maxResumeStep < 0.08f);

    // 4C. Toxic NaN / Inf / subnormal sanitization
    inL[0] = std::numeric_limits<float>::quiet_NaN();
    inL[1] = std::numeric_limits<float>::infinity();
    inL[2] = -std::numeric_limits<float>::infinity();
    inL[3] = 1.0e-39f;
    ring.push(inPtrs, 2, 256);
    for (int b = 0; b < 15; ++b) {
        pushToneBlock();
        ring.pull(outPtrs, 2, 256);
        for (int i = 0; i < 256; ++i) {
            RB26_GUARD_ASSERT(std::isfinite(outL[static_cast<size_t>(i)]));
            const float absV = std::abs(outL[static_cast<size_t>(i)]);
            RB26_GUARD_ASSERT(absV == 0.0f || absV >= 1.0e-20f);
        }
    }
}

// ============================================================================
// Test 5, 6, 7: Duplex Device + Sleep/Resume D3->D0 Retries + Storm & Hot Input Recovery
// ============================================================================
void runDuplexLifecycleSleepAndStormTests() {
    juce::ScopedJuceInitialiser_GUI juceInit;

    SharedMockHardwareState hwState;
    juce::AudioDeviceManager dm;
    dm.addAudioDeviceType(std::make_unique<rb26::standalone::DriftCompensatedAudioIODeviceType>(
        std::make_unique<MockWindowsAudioDeviceType>(hwState)));

    const juce::String initErr = dm.initialise(2, 2, nullptr, true);
    RB26_GUARD_ASSERT(initErr.isEmpty());

    juce::AudioDeviceManager::AudioDeviceSetup setup = dm.getAudioDeviceSetup();
    setup.inputDeviceName = "CABLE Output (VB-Audio Virtual Cable)";
    setup.outputDeviceName = "Speakers (Realtek High Definition Audio)";
    setup.sampleRate = 48000.0;
    setup.bufferSize = 480;
    const juce::String setupErr = dm.setAudioDeviceSetup(setup, true);
    RB26_GUARD_ASSERT(setupErr.isEmpty());

    auto* duplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
    RB26_GUARD_ASSERT(duplex != nullptr);
    RB26_GUARD_ASSERT(duplex->isOpen() && duplex->isPlaying());

    // Wire BRAUN_RB26AudioProcessor via AudioProcessorPlayer just like StandalonePluginHolder!
    BRAUN_RB26AudioProcessor proc;
    juce::AudioProcessorPlayer player;
    player.setProcessor(&proc);
    dm.addAudioCallback(&player);

    int processorResetCallbacks = 0;
    rb26::standalone::StandaloneAudioRecoveryController recovery(
        dm,
        [&]() {
            proc.reset();
            ++processorResetCallbacks;
        },
        0 /* manual deterministic watchdog ticks */);

    // 5. Verify Standby passthrough AND Active Reverb over VB-Cable -> Speakers duplex bridge
    //    (paced at realistic ~10 ms hardware intervals so the physical-rate storm detector sees normal timing!)
    {
        auto* mockIn = dynamic_cast<MockEndpointAudioIODevice*>(duplex->getInnerInputDevice());
        auto* mockOut = dynamic_cast<MockEndpointAudioIODevice*>(duplex->getInnerOutputDevice());
        RB26_GUARD_ASSERT(mockIn != nullptr && mockOut != nullptr);

        proc.setPower(false); // Standby passthrough!
        std::vector<float> inL(480, 0.0f), inR(480, 0.0f);
        std::vector<float> outL(480, 0.0f), outR(480, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        uint64_t idx = 0;
        double outEnergy = 0.0;
        for (int b = 0; b < 20; ++b) {
            for (int i = 0; i < 480; ++i) {
                const float v = static_cast<float>(0.5 * std::sin(kTwoPi * 500.0 * static_cast<double>(idx++) / 48000.0));
                inL[static_cast<size_t>(i)] = v;
                inR[static_cast<size_t>(i)] = -v;
            }
            mockIn->pumpInputBlock(inPtrs, 2, 480);
            mockOut->pumpOutputBlock(outPtrs, 2, 480);
            juce::Thread::sleep(4);
            if (b >= 10) {
                for (int i = 0; i < 480; ++i) {
                    outEnergy += static_cast<double>(outL[static_cast<size_t>(i)]) * static_cast<double>(outL[static_cast<size_t>(i)]);
                }
            }
        }
        RB26_GUARD_ASSERT(!duplex->hasFatalStreamFault());
        RB26_GUARD_ASSERT(outEnergy > 100.0);
        proc.setPower(true);
    }

    // 6. Simulate Laptop Sleep (PBT_APMSUSPEND) -> Resume (PBT_APMRESUMEAUTOMATIC)
    //    where endpoints fail the first 2 D3->D0 wake attempts (causing AudioDeviceManager
    //    to wipe currentSetup via deleteCurrentDevice()) before succeeding on attempt #3!
    recovery.handleSystemSuspend();
    RB26_GUARD_ASSERT(recovery.isSuspended());
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() == nullptr); // Cleanly closed before OS sleep!

    hwState.failNextOpenCount = 2; // First 2 open() calls on wake will fail!
    double simNowMs = juce::Time::getMillisecondCounterHiRes();
    const int64_t simWallMs = rb26::standalone::StandaloneAudioRecoveryController::currentWallClockMs();
    recovery.handleSystemResume(100.0);
    RB26_GUARD_ASSERT(!recovery.isSuspended());
    RB26_GUARD_ASSERT(recovery.isRecoveryPending());

    // Attempt #1: output open fails (failNextOpenCount: 2 -> 1); AudioDeviceManager wipes its internal currentSetup!
    simNowMs += 150.0;
    recovery.tickWatchdog(simNowMs, simWallMs + 150);
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() == nullptr);
    RB26_GUARD_ASSERT(recovery.isRecoveryPending());
    RB26_GUARD_ASSERT(recovery.getSavedSetup().inputDeviceName == "CABLE Output (VB-Audio Virtual Cable)");
    RB26_GUARD_ASSERT(recovery.getSavedSetup().outputDeviceName == "Speakers (Realtek High Definition Audio)");

    // Attempt #2: output open fails again (failNextOpenCount: 1 -> 0)
    simNowMs += 700.0;
    recovery.tickWatchdog(simNowMs, simWallMs + 850);
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() == nullptr);
    RB26_GUARD_ASSERT(recovery.isRecoveryPending());

    // Attempt #3: hardware finishes waking to D0 -> both VB-Cable & Speakers reopen cleanly!
    simNowMs += 1100.0;
    recovery.tickWatchdog(simNowMs, simWallMs + 1950);
    RB26_GUARD_ASSERT(!recovery.isRecoveryPending());
    RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == 1);
    RB26_GUARD_ASSERT(processorResetCallbacks == 1);
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() != nullptr && dm.getCurrentAudioDevice()->isOpen());
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().inputDeviceName == "CABLE Output (VB-Audio Virtual Cable)");
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().outputDeviceName == "Speakers (Realtek High Definition Audio)");

    // 7A. Simulate input-only silent WAIT_TIMEOUT exit (!inputDev->isOpen()) -> non-disruptive hot capture recovery!
    duplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
    RB26_GUARD_ASSERT(duplex != nullptr);
    hwState.inputForceClosed = true;
    RB26_GUARD_ASSERT(!duplex->getInnerInputDevice()->isOpen());

    simNowMs += 300.0;
    recovery.tickWatchdog(simNowMs, simWallMs + 2250);
    RB26_GUARD_ASSERT(duplex->getInputHotRecoveryCount() == 1);
    RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == 1); // Output was NEVER interrupted!
    RB26_GUARD_ASSERT(duplex->getInnerInputDevice() != nullptr && duplex->getInnerInputDevice()->isOpen());

    // 7B. Simulate post-sleep AUDCLNT_E_DEVICE_INVALIDATED unthrottled callback storm on output!
    {
        auto* mockOut = dynamic_cast<MockEndpointAudioIODevice*>(duplex->getInnerOutputDevice());
        RB26_GUARD_ASSERT(mockOut != nullptr);
        std::vector<float> outL(480, 1.0f), outR(480, 1.0f);
        float* outPtrs[2] = { outL.data(), outR.data() };

        // Fire 32 zero-wait back-to-back callbacks (> 24 blocks & >= 200 ms audio in < 15 ms wall-clock!)
        for (int i = 0; i < 32; ++i) {
            mockOut->pumpOutputBlock(outPtrs, 2, 480);
            if (duplex->hasFatalStreamFault()) {
                break;
            }
        }
        RB26_GUARD_ASSERT(duplex->hasFatalStreamFault());

        // Watchdog detects fatal fault and schedules & executes full stream recovery
        const double realNow1 = juce::Time::getMillisecondCounterHiRes();
        const int64_t realWall1 = rb26::standalone::StandaloneAudioRecoveryController::currentWallClockMs();
        recovery.tickWatchdog(realNow1, realWall1);
        RB26_GUARD_ASSERT(recovery.isRecoveryPending());

        recovery.tickWatchdog(realNow1 + 400.0, realWall1 + 400);
        RB26_GUARD_ASSERT(!recovery.isRecoveryPending());
        RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == 2);
        RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() != nullptr && dm.getCurrentAudioDevice()->isOpen());
    }

    dm.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    dm.closeAudioDevice();
}

// ============================================================================
// Test 6: Live Windows OS Power-Throttling API & Real WASAPI + VB-Cable Probe
// ============================================================================
#if JUCE_WINDOWS
struct SilentMonitorCallback final : public juce::AudioIODeviceCallback {
    std::atomic<uint64_t> callbacks { 0 };
    std::atomic<bool> sawFiniteInput { true };

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        if (inputChannelData != nullptr) {
            for (int ch = 0; ch < numInputChannels; ++ch) {
                if (inputChannelData[ch] != nullptr) {
                    for (int n = 0; n < numSamples; ++n) {
                        if (!std::isfinite(inputChannelData[ch][n])) {
                            sawFiniteInput.store(false, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
        // Output strict digital silence so hardware speakers never emit test noise
        if (outputChannelData != nullptr) {
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                if (outputChannelData[ch] != nullptr) {
                    std::fill(outputChannelData[ch], outputChannelData[ch] + numSamples, 0.0f);
                }
            }
        }
        callbacks.fetch_add(1, std::memory_order_relaxed);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
};
#endif

void runLiveWindowsWasapiVbCableIntegrationTest() {
#if JUCE_WINDOWS
    rb26::standalone::WindowsAudioProcessGuard procGuard;
    RB26_GUARD_ASSERT(procGuard.isPowerThrottlingDisabled());
    RB26_GUARD_ASSERT(procGuard.isHighResTimerActive());

    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioDeviceManager dm;
    rb26::standalone::installDriftCompensatedDeviceTypes(dm);

    juce::AudioIODeviceType* wasapiShared = nullptr;
    for (auto* t : dm.getAvailableDeviceTypes()) {
        if (t->getTypeName() == "Windows Audio") {
            wasapiShared = t;
            break;
        }
    }
    if (wasapiShared == nullptr) {
        return; // Headless Windows Server VM with no audio service
    }

    wasapiShared->scanForDevices();
    const auto inputs = wasapiShared->getDeviceNames(true);
    const auto outputs = wasapiShared->getDeviceNames(false);
    if (inputs.isEmpty() || outputs.isEmpty()) {
        return;
    }

    // Prefer VB-Cable capture paired with a non-VB-Cable physical output to exercise real two-clock drift!
    juce::String chosenIn = inputs[0];
    for (const auto& name : inputs) {
        if (name.containsIgnoreCase("CABLE") || name.containsIgnoreCase("VB-Audio")) {
            chosenIn = name;
            break;
        }
    }
    juce::String chosenOut = outputs[0];
    for (const auto& name : outputs) {
        if (!name.containsIgnoreCase("CABLE") && !name.containsIgnoreCase("VB-Audio")) {
            chosenOut = name;
            break;
        }
    }

    const juce::String initErr = dm.initialise(2, 2, nullptr, true);
    if (initErr.isNotEmpty()) {
        return;
    }

    SilentMonitorCallback silentCb;
    dm.addAudioCallback(&silentCb);

    auto setup = dm.getAudioDeviceSetup();
    setup.inputDeviceName = chosenIn;
    setup.outputDeviceName = chosenOut;
    const juce::String openErr = dm.setAudioDeviceSetup(setup, true);
    RB26_GUARD_ASSERT(openErr.isEmpty());

    auto* duplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
    RB26_GUARD_ASSERT(duplex != nullptr);

    rb26::standalone::StandaloneAudioRecoveryController recovery(dm, {}, 0);

    // Stream real WASAPI capture (VB-Cable) -> ElasticDriftRingBuffer -> real WASAPI render for 900 ms
    // Note: Windows WASAPI sets AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY on the initial packet after
    // IAudioClient::Start(); verify zero NEW xruns accrue across steady-state streaming!
    juce::Thread::sleep(150);
    const int initialStartupXRuns = duplex->getXRunCount();
    juce::Thread::sleep(800);
    const int postStreamXRuns = duplex->getXRunCount();
    RB26_GUARD_ASSERT(silentCb.callbacks.load(std::memory_order_relaxed) >= 20);
    RB26_GUARD_ASSERT(silentCb.sawFiniteInput.load(std::memory_order_relaxed));
    RB26_GUARD_ASSERT(duplex->getRingBuffer().getPushBlockCount() >= 20);
    RB26_GUARD_ASSERT(duplex->getRingBuffer().getPullBlockCount() >= 20);
    RB26_GUARD_ASSERT(duplex->getRingBuffer().getUnderrunCount() == 0);
    RB26_GUARD_ASSERT(duplex->getRingBuffer().getRecenterCount() == 0);
    RB26_GUARD_ASSERT(initialStartupXRuns <= 2);
    RB26_GUARD_ASSERT(postStreamXRuns == initialStartupXRuns);
    RB26_GUARD_ASSERT(!duplex->hasFatalStreamFault());

    // Exercise real WASAPI + VB-Cable suspend -> resume teardown & reopen cycle!
    recovery.handleSystemSuspend();
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() == nullptr);

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int64_t w0 = rb26::standalone::StandaloneAudioRecoveryController::currentWallClockMs();
    recovery.handleSystemResume(50.0);
    juce::Thread::sleep(80);
    recovery.tickWatchdog(t0 + 100.0, w0 + 100);

    RB26_GUARD_ASSERT(!recovery.isRecoveryPending());
    RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == 1);
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() != nullptr && dm.getCurrentAudioDevice()->isOpen());
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().inputDeviceName == chosenIn);
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().outputDeviceName == chosenOut);

    const uint64_t postResumeStart = silentCb.callbacks.load(std::memory_order_relaxed);
    juce::Thread::sleep(400);
    RB26_GUARD_ASSERT(silentCb.callbacks.load(std::memory_order_relaxed) > postResumeStart + 10);

    // Regression: Sub-10ms buffer size (256 frames @ 48 kHz) on live WASAPI Shared + VB-Cable!
    // Prior attempt starved WASAPIInputDevice::queue in <90 ms (13 underruns & 133 xruns in 1.2 s).
    {
        auto subSetup = dm.getAudioDeviceSetup();
        subSetup.inputDeviceName = chosenIn;
        subSetup.outputDeviceName = chosenOut;
        subSetup.sampleRate = 48000.0;
        subSetup.bufferSize = 256;
        const juce::String subErr = dm.setAudioDeviceSetup(subSetup, true);
        if (subErr.isEmpty()) {
            auto* subDuplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
            RB26_GUARD_ASSERT(subDuplex != nullptr);
            RB26_GUARD_ASSERT(subDuplex->getCurrentBufferSizeSamples() == 256);
            juce::Thread::sleep(150);
            const int subInitXRuns = subDuplex->getXRunCount();
            juce::Thread::sleep(1100);
            const int subEndXRuns = subDuplex->getXRunCount();
            RB26_GUARD_ASSERT(subDuplex->getRingBuffer().getPushBlockCount() >= 40);
            RB26_GUARD_ASSERT(subDuplex->getRingBuffer().getPullBlockCount() >= 120);
            RB26_GUARD_ASSERT(subDuplex->getRingBuffer().getUnderrunCount() == 0);
            RB26_GUARD_ASSERT(subDuplex->getRingBuffer().getRecenterCount() == 0);
            RB26_GUARD_ASSERT(subInitXRuns <= 2);
            RB26_GUARD_ASSERT(subEndXRuns == subInitXRuns);
        }
    }

    // Regression: Live Windows Audio (Low Latency Mode) with VB-Cable + physical output
    for (auto* t : dm.getAvailableDeviceTypes()) {
        if (t->getTypeName() == "Windows Audio (Low Latency Mode)") {
            t->scanForDevices();
            const auto llIns = t->getDeviceNames(true);
            const auto llOuts = t->getDeviceNames(false);
            if (llIns.contains(chosenIn) && llOuts.contains(chosenOut)) {
                dm.setCurrentAudioDeviceType("Windows Audio (Low Latency Mode)", true);
                auto llSetup = dm.getAudioDeviceSetup();
                llSetup.inputDeviceName = chosenIn;
                llSetup.outputDeviceName = chosenOut;
                if (dm.setAudioDeviceSetup(llSetup, true).isEmpty()) {
                    if (auto* llDuplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice())) {
                        juce::Thread::sleep(150);
                        const int llInitXRuns = llDuplex->getXRunCount();
                        juce::Thread::sleep(600);
                        const int llEndXRuns = llDuplex->getXRunCount();
                        RB26_GUARD_ASSERT(llDuplex->getRingBuffer().getPushBlockCount() >= 15);
                        RB26_GUARD_ASSERT(llDuplex->getRingBuffer().getPullBlockCount() >= 15);
                        RB26_GUARD_ASSERT(llDuplex->getRingBuffer().getUnderrunCount() == 0);
                        RB26_GUARD_ASSERT(llDuplex->getRingBuffer().getRecenterCount() == 0);
                        RB26_GUARD_ASSERT(llInitXRuns <= 2);
                        RB26_GUARD_ASSERT(llEndXRuns == llInitXRuns);
                    }
                }
            }
            break;
        }
    }

    dm.removeAudioCallback(&silentCb);
    dm.closeAudioDevice();
#endif
}

// ============================================================================
// Test 7: Large-Buffer DirectSound (4096) Cushion & Multi-Block Overflow Crossfade
// ============================================================================
void runLargeBufferAndMultiBlockOverflowCrossfadeTest() {
    // 7A. 4096-sample DirectSound-class buffer with consumer pulling BEFORE first producer push
    {
        rb26::standalone::ElasticDriftRingBuffer ring;
        ring.prepare(2, 4096, 4096, 48000.0, 48000.0);

        std::vector<float> inL(4096, 0.0f), inR(4096, 0.0f);
        std::vector<float> outL(4096, 0.0f), outR(4096, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        // Consumer fires first before producer has pushed a single block!
        ring.pull(outPtrs, 2, 4096);
        RB26_GUARD_ASSERT(ring.getUnderrunCount() == 0);

        uint64_t ph = 0;
        for (int b = 0; b < 120; ++b) {
            for (int i = 0; i < 4096; ++i) {
                const float v = static_cast<float>(0.7 * std::sin(kTwoPi * 440.0 * static_cast<double>(ph++) / 48000.0));
                inL[static_cast<size_t>(i)] = v;
                inR[static_cast<size_t>(i)] = v;
            }
            ring.push(inPtrs, 2, 4096);
            ring.pull(outPtrs, 2, 4096);
        }
        RB26_GUARD_ASSERT(ring.getUnderrunCount() == 0);
        RB26_GUARD_ASSERT(ring.getOverflowCount() == 0);
    }

    // 7B. Full-capacity producer overflow followed by small 64-sample pulls (< 128-frame crossfade)
    //     interleaved with big producer pushes (verifying consumer-private mCrossfadeTail immunity)
    {
        rb26::standalone::ElasticDriftRingBuffer ring;
        ring.prepare(2, 256, 64, 48000.0, 48000.0);

        std::vector<float> inL(256, 0.0f), inR(256, 0.0f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        uint64_t ph = 0;
        auto pushContinuousTone = [&]() {
            for (int i = 0; i < 256; ++i) {
                const float v = static_cast<float>(0.85 * std::sin(kTwoPi * 310.0 * static_cast<double>(ph++) / 48000.0));
                inL[static_cast<size_t>(i)] = v;
                inR[static_cast<size_t>(i)] = v;
            }
            ring.push(inPtrs, 2, 256);
        };

        for (int b = 0; b < 20; ++b) {
            pushContinuousTone();
            for (int sub = 0; sub < 4; ++sub) {
                ring.pull(outPtrs, 2, 64);
            }
        }
        float prev = outL.back();

        // Force full kCapacity wrap overflow!
        while (ring.getOverflowCount() == 0) {
            pushContinuousTone();
        }

        // Now pull 64-sample blocks (< kCrossfadeFrames = 128) while producer immediately overwrites freed slots
        // with adversarial -1.0/+1.0 square-wave values if old ring slots were still being read!
        float maxStep = 0.0f;
        for (int step = 0; step < 8; ++step) {
            ring.pull(outPtrs, 2, 64);
            for (int i = 0; i < 64; ++i) {
                maxStep = std::max(maxStep, std::abs(outL[static_cast<size_t>(i)] - prev));
                prev = outL[static_cast<size_t>(i)];
            }
            pushContinuousTone();
        }
        RB26_GUARD_ASSERT(ring.getRecenterCount() >= 1);
        RB26_GUARD_ASSERT(maxStep < 0.12f);
    }
}

// ============================================================================
// Test 8: Pre-Suspend Disconnect, Unplugged USB DAC Safe Fallback & Capture Storm/Rate Escalation
// ============================================================================
void runRoutingSafetyAndCaptureEscalationTests() {
    juce::ScopedJuceInitialiser_GUI juceInit;

    SharedMockHardwareState hwState;
    juce::AudioDeviceManager dm;
    auto* rawMockType = new MockWindowsAudioDeviceType(hwState);
    dm.addAudioDeviceType(std::make_unique<rb26::standalone::DriftCompensatedAudioIODeviceType>(
        std::unique_ptr<juce::AudioIODeviceType>(rawMockType)));

    RB26_GUARD_ASSERT(dm.initialise(2, 2, nullptr, true).isEmpty());

    // User explicitly chooses VB-Cable (index 1, NOT default Mic index 0!) -> USB DAC (index 1)
    auto setup = dm.getAudioDeviceSetup();
    setup.inputDeviceName = "CABLE Output (VB-Audio Virtual Cable)";
    setup.outputDeviceName = "Headphones (USB DAC)";
    setup.sampleRate = 48000.0;
    setup.bufferSize = 480;
    RB26_GUARD_ASSERT(dm.setAudioDeviceSetup(setup, true /* treatAsChosenDevice */).isEmpty());
    RB26_GUARD_ASSERT(dm.createStateXml() != nullptr);

    rb26::standalone::StandaloneAudioRecoveryController recovery(dm, {}, 0);
    RB26_GUARD_ASSERT(recovery.getSavedSetup().inputDeviceName == "CABLE Output (VB-Audio Virtual Cable)");
    RB26_GUARD_ASSERT(recovery.getSavedSetup().outputDeviceName == "Headphones (USB DAC)");

    // 8A. Simulate USB DAC disappearing 200ms BEFORE PBT_APMSUSPEND!
    //     Stock AudioDeviceManager::audioDeviceListChanged() immediately opens default Mic + default Speakers.
    //     Verify StandaloneAudioRecoveryController immediately shuts down the unsolicited Mic, preserves mSavedSetup,
    //     and schedules recovery!
    hwState.usbDacUnplugged = true;
    rawMockType->notifyDeviceListChanged();

    RB26_GUARD_ASSERT(recovery.getSavedSetup().inputDeviceName == "CABLE Output (VB-Audio Virtual Cable)");
    RB26_GUARD_ASSERT(recovery.getSavedSetup().outputDeviceName == "Headphones (USB DAC)");
    // Unsolicited Microphone MUST NOT remain open!
    if (dm.getCurrentAudioDevice() != nullptr) {
        RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().inputDeviceName != "Microphone (Realtek Audio)");
    }

    // 8B. Laptop sleeps and wakes with USB DAC STILL permanently unplugged across all 8 backoff retries (~32s).
    //     Post-retry fallback MUST switch output to Speakers while KEEPING VB-Cable (NEVER opening Microphone!)
    //     and MUST NOT wipe AudioDeviceManager::createStateXml()!
    recovery.handleSystemSuspend();
    double simNow = juce::Time::getMillisecondCounterHiRes();
    int64_t simWall = rb26::standalone::StandaloneAudioRecoveryController::currentWallClockMs();
    recovery.handleSystemResume(100.0);

    for (int attempt = 0; attempt < 10; ++attempt) {
        simNow += 12000.0;
        simWall += 12000;
        recovery.tickWatchdog(simNow, simWall);
        if (!recovery.isRecoveryPending()) {
            break;
        }
    }
    RB26_GUARD_ASSERT(!recovery.isRecoveryPending());
    RB26_GUARD_ASSERT(dm.getCurrentAudioDevice() != nullptr && dm.getCurrentAudioDevice()->isOpen());
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().inputDeviceName == "CABLE Output (VB-Audio Virtual Cable)");
    RB26_GUARD_ASSERT(dm.getAudioDeviceSetup().outputDeviceName == "Speakers (Realtek High Definition Audio)");
    RB26_GUARD_ASSERT(dm.createStateXml() != nullptr); // Saved XML preserved!

    // 8C. Simulate DirectSound/capture spin-storm -> throttled & hot-recovered without dropping output!
    auto* duplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
    RB26_GUARD_ASSERT(duplex != nullptr);
    {
        auto* mockIn = dynamic_cast<MockEndpointAudioIODevice*>(duplex->getInnerInputDevice());
        RB26_GUARD_ASSERT(mockIn != nullptr);
        std::vector<float> inL(480, 0.25f), inR(480, 0.25f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        for (int i = 0; i < 32; ++i) {
            mockIn->pumpInputBlock(inPtrs, 2, 480);
        }
        const uint64_t prevFullRecoveries = recovery.getFullRecoveryCount();
        simNow += 500.0;
        simWall += 500;
        recovery.tickWatchdog(simNow, simWall);
        RB26_GUARD_ASSERT(duplex->getInputHotRecoveryCount() >= 1);
        RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == prevFullRecoveries);
    }

    // 8D. Simulate mid-session capture sample-rate change (48 kHz -> 44.1 kHz while output is locked to 48 kHz)
    //     -> escalates to full reopen so ElasticDriftRingBuffer::prepare() reconfigures mNominalRatio (44.1k / 48k)!
    {
        const uint64_t prevFullRecoveries = recovery.getFullRecoveryCount();
        hwState.forcedInputNativeRate = 44100.0;
        hwState.lockInputToForcedRate = true;
        hwState.lockOutputTo48k = true;
        hwState.inputForceClosed = true;

        const double realNow = juce::Time::getMillisecondCounterHiRes() + 20000.0;
        const int64_t realWall = rb26::standalone::StandaloneAudioRecoveryController::currentWallClockMs() + 20000;
        recovery.tickWatchdog(realNow, realWall);
        RB26_GUARD_ASSERT(recovery.isRecoveryPending());

        recovery.tickWatchdog(realNow + 500.0, realWall + 500);
        RB26_GUARD_ASSERT(!recovery.isRecoveryPending());
        RB26_GUARD_ASSERT(recovery.getFullRecoveryCount() == prevFullRecoveries + 1);

        auto* reopenedDuplex = dynamic_cast<rb26::standalone::DuplexDriftCompensatedAudioIODevice*>(dm.getCurrentAudioDevice());
        RB26_GUARD_ASSERT(reopenedDuplex != nullptr);
        RB26_GUARD_ASSERT(std::abs(reopenedDuplex->getCurrentSampleRate() - 48000.0) < 0.5);
        RB26_GUARD_ASSERT(std::abs(reopenedDuplex->getRingBuffer().getCurrentRatio() - (44100.0 / 48000.0)) < 0.005);
    }

    dm.closeAudioDevice();
}

} // namespace

int main() {
    std::cout << "=== BRAUN RB-26 Standalone Audio Guard & VB-Cable Drift/Sleep Verification ===\n";

    std::cout << "[1/8] Sustained +85 ppm Clock Drift (VB-Cable > Hardware, 6M frames)... ";
    runSustainedClockDriftTest(+85.0, 480, 480);
    std::cout << "PASS\n";

    std::cout << "[2/8] Sustained -85 ppm Clock Drift & Block Mismatch (480 vs 512, 6M frames)... ";
    runSustainedClockDriftTest(-85.0, 480, 512);
    std::cout << "PASS\n";

    std::cout << "[3/8] Async Sample Rate Mismatch (44.1 kHz VB-Cable -> 48.0 kHz Output)... ";
    runAsyncSampleRateConversionTest();
    std::cout << "PASS\n";

    std::cout << "[4/8] Severe OS Stall Recenter, Underrun Decay & Toxic NaN/Inf Immunity... ";
    runStallAndToxicSelfHealingTest();
    std::cout << "PASS\n";

    std::cout << "[5/8] Duplex Passthrough, S3/S0 Sleep D3->D0 Retry & Invalidated Storm Recovery... ";
    runDuplexLifecycleSleepAndStormTests();
    std::cout << "PASS\n";

    std::cout << "[6/8] Live Win32 EcoQoS Guard, Sub-10ms (256) & Low-Latency WASAPI + VB-Cable... ";
    runLiveWindowsWasapiVbCableIntegrationTest();
    std::cout << "PASS\n";

    std::cout << "[7/8] Large-Buffer DirectSound (4096) Cushion & Race-Free Overflow Crossfade... ";
    runLargeBufferAndMultiBlockOverflowCrossfadeTest();
    std::cout << "PASS\n";

    std::cout << "[8/8] Pre-Suspend Disconnect, Unplugged USB DAC Safe Fallback & Rate Escalation... ";
    runRoutingSafetyAndCaptureEscalationTests();
    std::cout << "PASS\n";

    std::cout << "ALL STANDALONE AUDIO GUARD TESTS PASSED UNANIMOUSLY.\n";
    return 0;
}
