#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone

#include "StandaloneAudioGuard.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace rb26::standalone {

class Rb26StandalonePluginHolder final : public juce::StandalonePluginHolder {
public:
    Rb26StandalonePluginHolder(juce::PropertySet* settingsToUse,
                               bool takeOwnershipOfSettings,
                               const juce::String& preferredDefaultDeviceName,
                               const juce::AudioDeviceManager::AudioDeviceSetup* preferredSetupOptions,
                               const juce::Array<PluginInOuts>& channels,
                               bool shouldAutoOpenMidiDevices)
        // Pass {0, 0} dummy channel config and nullptr settings to base constructor so JUCE's
        // base init() performs a zero-channel no-op without opening raw uncompensated WASAPI devices.
        : juce::StandalonePluginHolder(nullptr,
                                       false,
                                       preferredDefaultDeviceName,
                                       preferredSetupOptions,
                                       juce::Array<PluginInOuts>{ PluginInOuts{ 0, 0 } },
                                       shouldAutoOpenMidiDevices) {
        stopPlaying();

        // Restore real channel configuration & feedback-loop protection flag
        channelConfiguration = channels;
        processorHasPotentialFeedbackLoop = (getNumInputChannels() > 0 && getNumOutputChannels() > 0);

        // Wrap separate-clock Windows audio device types (WASAPI Shared/LowLatency/Exclusive & DirectSound)
        // with lock-free Hermite drift-compensated duplex bridges before opening any hardware device.
        installDriftCompensatedDeviceTypes(deviceManager);

        // Attach persistent user settings and open the configured audio/MIDI devices exactly once
        settings.set(settingsToUse, takeOwnershipOfSettings);

        const int inChannels = getNumInputChannels();
        const bool audioInputRequired = (inChannels > 0);

        auto finishBoot = [this, preferredDefaultDeviceName](bool enableInput) {
            reloadAudioDeviceState(enableInput, preferredDefaultDeviceName, options.get());
            reloadPluginState();
            startPlaying();

            mRecoveryController = std::make_unique<StandaloneAudioRecoveryController>(
                deviceManager,
                [this] {
                    if (processor != nullptr) {
                        processor->reset();
                    }
                });
        };

        if (audioInputRequired
            && juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
            && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
            juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, finishBoot);
        } else {
            finishBoot(audioInputRequired);
        }
    }

    ~Rb26StandalonePluginHolder() override {
        mRecoveryController.reset();
    }

private:
    WindowsAudioProcessGuard mProcessGuard;
    std::unique_ptr<StandaloneAudioRecoveryController> mRecoveryController;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rb26StandalonePluginHolder)
};

class Rb26StandaloneFilterApp final : public juce::JUCEApplication {
public:
    Rb26StandaloneFilterApp() {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = juce::CharPointer_UTF8(JucePlugin_Name);
        opts.filenameSuffix      = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
#if JUCE_LINUX || JUCE_BSD
        opts.folderName          = "~/.config";
#else
        opts.folderName          = "";
#endif
        mAppProperties.setStorageParameters(opts);
    }

    const juce::String getApplicationName() override { return juce::CharPointer_UTF8(JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted(const juce::String&) override {}

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder() {
        constexpr bool autoOpenMidi =
#if (JUCE_ANDROID || JUCE_IOS) && !JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
#else
            false;
#endif

#ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(channels, juce::numElementsInArray(channels));
#else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
#endif

        return std::make_unique<Rb26StandalonePluginHolder>(mAppProperties.getUserSettings(),
                                                            false,
                                                            juce::String{},
                                                            nullptr,
                                                            channelConfig,
                                                            autoOpenMidi);
    }

    juce::StandaloneFilterWindow* createWindow() {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty()) {
            jassertfalse;
            return nullptr;
        }

        return new juce::StandaloneFilterWindow(
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    void initialise(const juce::String&) override {
        mMainWindow.reset(createWindow());
        if (mMainWindow != nullptr) {
#if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent(mMainWindow.get(), false);
#endif
            mMainWindow->setVisible(true);
        } else {
            mHeadlessPluginHolder = createPluginHolder();
        }
    }

    void shutdown() override {
        mHeadlessPluginHolder.reset();
        mMainWindow.reset();
        mAppProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override {
        if (mHeadlessPluginHolder != nullptr) {
            mHeadlessPluginHolder->savePluginState();
        }
        if (mMainWindow != nullptr && mMainWindow->pluginHolder != nullptr) {
            mMainWindow->pluginHolder->savePluginState();
        }

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents()) {
            juce::Timer::callAfterDelay(100, []() {
                if (auto* app = juce::JUCEApplicationBase::getInstance()) {
                    app->systemRequestedQuit();
                }
            });
        } else {
            quit();
        }
    }

private:
    juce::ApplicationProperties mAppProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mMainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> mHeadlessPluginHolder;
};

} // namespace rb26::standalone

JUCE_CREATE_APPLICATION_DEFINE(rb26::standalone::Rb26StandaloneFilterApp)

#endif // JucePlugin_Build_Standalone
