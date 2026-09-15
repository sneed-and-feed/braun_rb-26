#include "PluginProcessor.h"
#include "PluginEditor.h"

BRAUN_RB26AudioProcessor::BRAUN_RB26AudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", rb26::createParameterLayout())
{
    atomicPointers.initialize(apvts);
}

BRAUN_RB26AudioProcessor::~BRAUN_RB26AudioProcessor()
{
}

const juce::String BRAUN_RB26AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BRAUN_RB26AudioProcessor::acceptsMidi() const
{
    return true;
}

bool BRAUN_RB26AudioProcessor::producesMidi() const
{
    return false;
}

bool BRAUN_RB26AudioProcessor::isMidiEffect() const
{
    return false;
}

double BRAUN_RB26AudioProcessor::getTailLengthSeconds() const
{
    return 30.0;
}

int BRAUN_RB26AudioProcessor::getNumPrograms()
{
    return 1;
}

int BRAUN_RB26AudioProcessor::getCurrentProgram()
{
    return 0;
}

void BRAUN_RB26AudioProcessor::setCurrentProgram(int)
{
}

const juce::String BRAUN_RB26AudioProcessor::getProgramName(int)
{
    return "Default";
}

void BRAUN_RB26AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void BRAUN_RB26AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverbEngine.prepare(sampleRate, samplesPerBlock);
    exciterEngine.prepare(sampleRate);

    scopeWritePos.store(0, std::memory_order_relaxed);
    for (int i = 0; i < kScopeBufferSize; ++i)
    {
        scopeBufferL[i] = 0.0f;
        scopeBufferR[i] = 0.0f;
    }
}

void BRAUN_RB26AudioProcessor::releaseResources()
{
    reverbEngine.reset();
    exciterEngine.reset();
}

void BRAUN_RB26AudioProcessor::reset()
{
    reverbEngine.reset();
    exciterEngine.reset();
}

bool BRAUN_RB26AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    // Support Stereo -> Stereo, Mono -> Stereo, and Mono -> Mono
    if (mainOutput != juce::AudioChannelSet::stereo() && mainOutput != juce::AudioChannelSet::mono())
        return false;

    if (mainInput != juce::AudioChannelSet::stereo() && mainInput != juce::AudioChannelSet::mono() && !mainInput.isDisabled())
        return false;

    return true;
}

void BRAUN_RB26AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Tier 1 Denormal Protection: Hardware FTZ/DAZ via ScopedNoDenormals
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Process incoming MIDI Note-On and Note-Off events to control AcousticExciterEngine
    for (const auto metadata : midiMessages)
    {
        if (metadata.numBytes >= 3)
        {
            const auto* rawData = metadata.data;
            const uint8_t status = rawData[0] & 0xF0;
            const uint8_t note = rawData[1] & 0x7F;
            const uint8_t vel = rawData[2] & 0x7F;
            if (status == 0x90 && vel > 0)
            {
                exciterEngine.triggerVoice(static_cast<float>(note), static_cast<float>(vel) / 127.0f);
            }
            else if (status == 0x80 || (status == 0x90 && vel == 0))
            {
                exciterEngine.releaseVoice(static_cast<float>(note));
            }
        }
    }

    float* outChannels[2];
    outChannels[0] = buffer.getWritePointer(0);
    outChannels[1] = (numChannels > 1) ? buffer.getWritePointer(1) : outChannels[0];

    // Standby Power Gating: If powered down, output clean silence unless awakened by MIDI Note-On
    if (!isPoweredOn.load(std::memory_order_relaxed))
    {
        bool hasNoteOn = false;
        for (const auto metadata : midiMessages)
        {
            if (metadata.numBytes >= 3)
            {
                const auto* rawData = metadata.data;
                if ((rawData[0] & 0xF0) == 0x90 && rawData[2] > 0)
                {
                    hasNoteOn = true;
                    break;
                }
            }
        }

        if (hasNoteOn)
        {
            isPoweredOn.store(true, std::memory_order_relaxed);
        }
        else
        {
            buffer.clear();
            pushScopeSamples(outChannels[0], outChannels[1], numSamples);
            midiMessages.clear();
            return;
        }
    }

    // Wait-free POD snapshot load with std::memory_order_relaxed (0 locks, 0 memory allocs)
    const auto snapshot = atomicPointers.loadSnapshot();
    reverbEngine.setParameters(snapshot.toDspParams());

    // Prepare non-allocating stack pointers for channel routing
    const float* inChannels[2];
    inChannels[0] = buffer.getReadPointer(0);
    inChannels[1] = (numChannels > 1) ? buffer.getReadPointer(1) : inChannels[0];

    // Real-time block chunking for acoustic exciter + reverb integration (0 heap allocs)
    constexpr int kMaxChunk = 512;
    int samplesProcessed = 0;

    while (samplesProcessed < numSamples)
    {
        const int chunkSize = std::min(kMaxChunk, numSamples - samplesProcessed);

        float exciterDirectL[kMaxChunk] = { 0.0f };
        float exciterDirectR[kMaxChunk] = { 0.0f };
        float exciterReverbL[kMaxChunk] = { 0.0f };
        float exciterReverbR[kMaxChunk] = { 0.0f };

        float* directBus[2] = { exciterDirectL, exciterDirectR };
        float* reverbBus[2] = { exciterReverbL, exciterReverbR };

        // Synthesize exciter voices, Poisson generative clock, chords, and lab pulses
        exciterEngine.process(directBus, reverbBus, chunkSize);

        const bool isReverbAndDirect = (exciterEngine.getParameters().routing == rb26::ExciterRouting::ReverbAndDirect);

        float combinedInL[kMaxChunk];
        float combinedInR[kMaxChunk];

        const float* extInL = inChannels[0] + samplesProcessed;
        const float* extInR = inChannels[1] + samplesProcessed;

        for (int i = 0; i < chunkSize; ++i)
        {
            // If ReverbAndDirect: route exciter into primary input bus for unified dry/wet mixing
            combinedInL[i] = extInL[i] + (isReverbAndDirect ? exciterDirectL[i] : 0.0f);
            combinedInR[i] = extInR[i] + (isReverbAndDirect ? exciterDirectR[i] : 0.0f);
        }

        const float* chunkIn[2] = { combinedInL, combinedInR };
        float* chunkOut[2] = { outChannels[0] + samplesProcessed, (numChannels > 1 ? outChannels[1] + samplesProcessed : nullptr) };

        // If ReverbOnly: route exciter directly into reverb tank aux input, bypassing dry path
        const float* auxReverb[2] = { exciterReverbL, exciterReverbR };
        const float* const* auxPtr = isReverbAndDirect ? nullptr : auxReverb;

        // Process core reverb engine
        reverbEngine.process(chunkIn, chunkOut, std::min(numChannels, 2), chunkSize, auxPtr);

        // Guarantee zero hard clipping: apply master soft limiter to final output bus
        for (int i = 0; i < chunkSize; ++i)
        {
            outChannels[0][samplesProcessed + i] = rb26::softLimit(outChannels[0][samplesProcessed + i]);
            if (numChannels > 1)
            {
                outChannels[1][samplesProcessed + i] = rb26::softLimit(outChannels[1][samplesProcessed + i]);
            }
        }

        samplesProcessed += chunkSize;
    }

    // Push processed audio frames to wait-free visualizer oscilloscope buffer
    pushScopeSamples(outChannels[0], outChannels[1], numSamples);

    // Audio effects do not produce MIDI messages
    midiMessages.clear();
}

void BRAUN_RB26AudioProcessor::pushScopeSamples(const float* left, const float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    int pos = scopeWritePos.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        scopeBufferL[pos] = left[i];
        scopeBufferR[pos] = (right != nullptr) ? right[i] : left[i];
        pos = (pos + 1);
        if (pos >= kScopeBufferSize)
            pos = 0;
    }
    scopeWritePos.store(pos, std::memory_order_release);
}

void BRAUN_RB26AudioProcessor::getScopeSamples(float* destL, float* destR, int numSamplesToRead) const noexcept
{
    if (destL == nullptr || numSamplesToRead <= 0)
        return;

    numSamplesToRead = std::min(numSamplesToRead, kScopeBufferSize);

    const int writePos = scopeWritePos.load(std::memory_order_acquire);
    int readPos = ((writePos - numSamplesToRead) % kScopeBufferSize + kScopeBufferSize) % kScopeBufferSize;
    for (int i = 0; i < numSamplesToRead; ++i)
    {
        destL[i] = scopeBufferL[readPos];
        if (destR != nullptr)
            destR[i] = scopeBufferR[readPos];
        readPos = (readPos + 1);
        if (readPos >= kScopeBufferSize)
            readPos = 0;
    }
}

bool BRAUN_RB26AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* BRAUN_RB26AudioProcessor::createEditor()
{
    return new BRAUN_RB26AudioProcessorEditor(*this);
}

void BRAUN_RB26AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BRAUN_RB26AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(vt);
    }
}

// JUCE plugin entry point export
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BRAUN_RB26AudioProcessor();
}
