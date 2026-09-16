#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "../dsp/Rb26Engine.h"
#include "../dsp/AcousticExciter.h"

class BRAUN_RB26AudioProcessor : public juce::AudioProcessor
{
public:
    BRAUN_RB26AudioProcessor();
    ~BRAUN_RB26AudioProcessor() override;

    // Hard real-time lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Audio callback: Guaranteed 0 heap allocations, 0 locks, denormals flush
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // Preset & State Serialization
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // APVTS & Engine Accessors
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    rb26::Rb26ReverbEngine& getReverbEngine() noexcept { return reverbEngine; }
    rb26::AcousticExciterEngine& getExciterEngine() noexcept { return exciterEngine; }
    const rb26::Rb26AtomicPointers& getAtomicPointers() const noexcept { return atomicPointers; }

    // Power lifecycle control
    void setPower(bool powered) noexcept {
        isPoweredOn.store(powered, std::memory_order_relaxed);
        mPendingEngineReset.store(true, std::memory_order_release);
    }
    bool isPower() const noexcept { return isPoweredOn.load(std::memory_order_relaxed); }

    // Lock-Free SPSC Telemetry Access
    bool popVisualizerFrame(rb26::Rb26ReverbEngine::VisualizerFrame& frame) noexcept {
        return reverbEngine.popVisualizerFrame(frame);
    }

    // Lock-Free Oscilloscope / Lissajous Visualizer Buffer
    static constexpr int kScopeBufferSize = 2048;
    void pushScopeSamples(const float* left, const float* right, int numSamples) noexcept;
    void getScopeSamples(float* destL, float* destR, int numSamplesToRead) const noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;
    rb26::Rb26ReverbEngine reverbEngine;
    rb26::AcousticExciterEngine exciterEngine;
    rb26::Rb26AtomicPointers atomicPointers;
    std::atomic<bool> isPoweredOn { true };
    std::atomic<bool> mPendingEngineReset { false };
    int mCurrentProgram { 0 };

    // Visualizer waveform circular buffer
    std::atomic<int> scopeWritePos { 0 };
    float scopeBufferL[kScopeBufferSize] {};
    float scopeBufferR[kScopeBufferSize] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BRAUN_RB26AudioProcessor)
};
