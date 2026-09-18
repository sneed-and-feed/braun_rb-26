#include "PluginProcessor.h"
#include "PluginEditor.h"

BRAUN_RB26AudioProcessor::BRAUN_RB26AudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", rb26::createParameterLayout())
{
    atomicPointers.initialize(apvts);
    recorderThread.startThread();
}

BRAUN_RB26AudioProcessor::~BRAUN_RB26AudioProcessor()
{
    stopRecording();
    recorderThread.stopThread(2000);
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
    return 10;
}

int BRAUN_RB26AudioProcessor::getCurrentProgram()
{
    return mCurrentProgram;
}

void BRAUN_RB26AudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    mCurrentProgram = index;

    static const auto presets = rb26::Rb26ReverbEngine::getFactoryPresets();
    if (static_cast<size_t>(index) >= presets.size())
        return;

    const auto& p = presets[static_cast<size_t>(index)].params;

    auto setParamFloat = [this](const juce::ParameterID& pid, float val) {
        if (auto* param = apvts.getParameter(pid.getParamID()))
            param->setValueNotifyingHost(std::clamp(param->convertTo0to1(val), 0.0f, 1.0f));
    };
    auto setParamChoice = [this](const juce::ParameterID& pid, float choiceIndex) {
        if (auto* param = apvts.getParameter(pid.getParamID()))
            param->setValueNotifyingHost(std::clamp(param->convertTo0to1(choiceIndex), 0.0f, 1.0f));
    };
    auto setParamBool = [this](const juce::ParameterID& pid, bool val) {
        if (auto* param = apvts.getParameter(pid.getParamID()))
            param->setValueNotifyingHost(val ? 1.0f : 0.0f);
    };

    setParamFloat(rb26::ParamIDs::inputTrimDb, p.inputTrimDb);
    setParamFloat(rb26::ParamIDs::preDelayMs, p.preDelayMs);
    setParamFloat(rb26::ParamIDs::dryWetMix, p.dryWetMix);
    setParamFloat(rb26::ParamIDs::earlyLateMix, p.earlyLateMix);
    setParamFloat(rb26::ParamIDs::lowCrossoverHz, p.lowCrossoverHz);
    setParamFloat(rb26::ParamIDs::bassRt60Mult, p.bassRt60Mult);
    setParamFloat(rb26::ParamIDs::punchDucking, p.punchDucking);
    setParamFloat(rb26::ParamIDs::subMonoHz, p.subMonoHz);
    setParamFloat(rb26::ParamIDs::roomSize, p.roomSize);
    setParamFloat(rb26::ParamIDs::decayRt60Sec, p.decayRt60Sec);
    setParamFloat(rb26::ParamIDs::highDampingHz, p.highDampingHz);
    setParamFloat(rb26::ParamIDs::diffusionDensity, p.diffusionDensity);
    setParamBool(rb26::ParamIDs::freezeHold, p.freezeHold);
    setParamFloat(rb26::ParamIDs::shimmerSend, p.shimmerSend);
    setParamFloat(rb26::ParamIDs::dimmerSend, p.dimmerSend);
    setParamChoice(rb26::ParamIDs::shimmerInterval, static_cast<float>(rb26::shimmerIndexFromInterval(p.shimmerInterval)));
    setParamChoice(rb26::ParamIDs::dimmerInterval, static_cast<float>(rb26::dimmerIndexFromInterval(p.dimmerInterval)));
    setParamFloat(rb26::ParamIDs::pitchBlend, p.pitchBlend);
    setParamFloat(rb26::ParamIDs::pitchFeedback, p.pitchFeedback);
    setParamFloat(rb26::ParamIDs::pitchDelayMs, p.pitchDelayMs);
    setParamFloat(rb26::ParamIDs::tailModRateHz, p.tailModRateHz);
    setParamFloat(rb26::ParamIDs::tailModDepthMs, p.tailModDepthMs);
    setParamFloat(rb26::ParamIDs::tailBloomMs, p.tailBloomMs);
    setParamFloat(rb26::ParamIDs::stereoWidth, p.stereoWidth);
    setParamFloat(rb26::ParamIDs::outputTrimDb, p.outputTrimDb);
    setParamBool(rb26::ParamIDs::limiterEnable, p.limiterEnable);
    setParamFloat(rb26::ParamIDs::pitchBoost, p.pitchBoostDb);
}

const juce::String BRAUN_RB26AudioProcessor::getProgramName(int index)
{
    static const auto presets = rb26::Rb26ReverbEngine::getFactoryPresets();
    if (index >= 0 && static_cast<size_t>(index) < presets.size())
    {
        return juce::String(presets[static_cast<size_t>(index)].name);
    }
    return "Default";
}

void BRAUN_RB26AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void BRAUN_RB26AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverbEngine.prepare(sampleRate, samplesPerBlock);
    exciterEngine.prepare(sampleRate);

    // Snapshot active APVTS parameters and initialize DSP state & smoothers immediately (BUG-JUCE-4)
    const auto snapshot = atomicPointers.loadSnapshot();
    reverbEngine.setParameters(snapshot.toDspParams());
    reverbEngine.reset();
    mPendingEngineReset.store(false, std::memory_order_release);

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
    mPendingEngineReset.store(true, std::memory_order_release);
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
            else if (status == 0xB0 && note == 64)
            {
                // MIDI CC 64 Sustain Pedal -> toggle Freeze Hold
                const bool sustainOn = (vel >= 64);
                if (auto* freezeParam = apvts.getParameter(rb26::ParamIDs::freezeHold.getParamID()))
                {
                    freezeParam->setValueNotifyingHost(sustainOn ? 1.0f : 0.0f);
                }
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
            if (mPendingEngineReset.exchange(false, std::memory_order_acq_rel))
            {
                reverbEngine.reset();
            }
            buffer.clear();
            pushScopeSamples(outChannels[0], outChannels[1], numSamples);
            midiMessages.clear();
            return;
        }
    }

    // Wait-free POD snapshot load with std::memory_order_relaxed (0 locks, 0 memory allocs)
    const auto snapshot = atomicPointers.loadSnapshot();
    reverbEngine.setParameters(snapshot.toDspParams());

    if (mPendingEngineReset.exchange(false, std::memory_order_acq_rel))
    {
        reverbEngine.reset();
    }

    // Prepare non-allocating stack pointers for channel routing
    const float* inChannels[2];
    inChannels[0] = buffer.getReadPointer(0);
    inChannels[1] = (getTotalNumInputChannels() > 1) ? buffer.getReadPointer(1) : inChannels[0];

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

        // Ensure denormals are flushed without duplicate soft limiting (handled by reverbEngine when limiter_enable is active)
        for (int i = 0; i < chunkSize; ++i)
        {
            outChannels[0][samplesProcessed + i] = rb26::flushDenormal(outChannels[0][samplesProcessed + i]);
            if (numChannels > 1)
            {
                outChannels[1][samplesProcessed + i] = rb26::flushDenormal(outChannels[1][samplesProcessed + i]);
            }
        }

        samplesProcessed += chunkSize;
    }

    // Push processed audio frames to wait-free visualizer oscilloscope buffer
    pushScopeSamples(outChannels[0], outChannels[1], numSamples);

    // Push processed audio to lossless WAV recorder if active (lock-free)
    if (activeWriter.load(std::memory_order_acquire) != nullptr)
    {
        activeWriterWorkers.fetch_add(1, std::memory_order_acquire);
        if (auto* writer = activeWriter.load(std::memory_order_acquire))
        {
            const float* channels[] = { outChannels[0], outChannels[1] };
            writer->write(channels, numSamples);
        }
        activeWriterWorkers.fetch_sub(1, std::memory_order_release);
    }

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

void BRAUN_RB26AudioProcessor::startRecording()
{
    const juce::ScopedLock sl(recorderLock);
    if (activeWriter.load(std::memory_order_relaxed) != nullptr || threadedWriter != nullptr)
        return;

    const double sampleRateToUse = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;

    auto musicDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userMusicDirectory);
    if (!musicDir.isDirectory() && !musicDir.createDirectory().wasOk())
    {
        musicDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userDocumentsDirectory);
        if (!musicDir.isDirectory() && !musicDir.createDirectory().wasOk())
            musicDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userHomeDirectory);
    }

    const auto recordingsDir = musicDir.getChildFile("Braun RB-26 Recordings");
    if (!recordingsDir.exists())
    {
        const auto result = recordingsDir.createDirectory();
        if (result.failed())
            return;
    }

    const juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d-%H-%M-%S");
    const juce::File wavFile = recordingsDir.getNonexistentChildFile("braun-rb26-" + timestamp, ".wav");

    if (auto stream = wavFile.createOutputStream())
    {
        juce::WavAudioFormat wavFormat;
        if (auto* rawWriter = wavFormat.createWriterFor(stream.get(), sampleRateToUse, 2, 16, {}, 0))
        {
            stream.release();
            lastRecordedFile = wavFile;
            threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(rawWriter, recorderThread, 131072);
            activeWriter.store(threadedWriter.get(), std::memory_order_release);
        }
    }
}

void BRAUN_RB26AudioProcessor::stopRecording()
{
    const juce::ScopedLock sl(recorderLock);
    if (activeWriter.load(std::memory_order_relaxed) == nullptr && threadedWriter == nullptr)
        return;

    {
        const juce::ScopedLock slCb(getCallbackLock());
        activeWriter.store(nullptr, std::memory_order_release);
    }

    while (activeWriterWorkers.load(std::memory_order_acquire) > 0)
    {
        juce::Thread::yield();
    }

    threadedWriter.reset();

    recordingSavedDirty.store(true, std::memory_order_relaxed);

    if (lastRecordedFile.existsAsFile() && getActiveEditor() != nullptr)
    {
        juce::Thread::launch([f = lastRecordedFile] { f.revealToUser(); });
    }
}

bool BRAUN_RB26AudioProcessor::isRecording() const noexcept
{
    return activeWriter.load(std::memory_order_relaxed) != nullptr;
}

juce::File BRAUN_RB26AudioProcessor::getLastRecordedFile() const
{
    const juce::ScopedLock sl(recorderLock);
    return lastRecordedFile;
}

bool BRAUN_RB26AudioProcessor::consumeRecordingSavedDirty() noexcept
{
    return recordingSavedDirty.exchange(false, std::memory_order_relaxed);
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
    state.setProperty("currentProgram", mCurrentProgram, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BRAUN_RB26AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        if (vt.hasProperty("currentProgram"))
            mCurrentProgram = static_cast<int>(vt.getProperty("currentProgram"));
        apvts.replaceState(vt);
    }
}

// JUCE plugin entry point export
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BRAUN_RB26AudioProcessor();
}
