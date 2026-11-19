#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#if JUCE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#ifdef small
#undef small
#endif
#ifdef interface
#undef interface
#endif
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif
#endif

namespace rb26::standalone {

// ============================================================================
// 1. Windows Process Anti-Throttling & High-Resolution Timer Guard
// ============================================================================
// Prevents Windows 10/11 EcoQoS / background power throttling from capping CPU
// clocks, parking cores, or coalescing WaitForSingleObject multimedia timers
// to 15.625 ms when BRAUN_RB26.exe is minimized, occluded, or left running
// unattended with VB-Cable passthrough audio.
class WindowsAudioProcessGuard {
public:
    WindowsAudioProcessGuard() noexcept {
#if JUCE_WINDOWS
        disableProcessPowerThrottling();
        elevateProcessPriorityIfNormal();
        if (::timeBeginPeriod(1) == TIMERR_NOERROR) {
            mTimerPeriodActive = true;
        }
#endif
    }

    ~WindowsAudioProcessGuard() noexcept {
#if JUCE_WINDOWS
        if (mTimerPeriodActive) {
            ::timeEndPeriod(1);
            mTimerPeriodActive = false;
        }
#endif
    }

    WindowsAudioProcessGuard(const WindowsAudioProcessGuard&) = delete;
    WindowsAudioProcessGuard& operator=(const WindowsAudioProcessGuard&) = delete;

    [[nodiscard]] bool isPowerThrottlingDisabled() const noexcept { return mPowerThrottlingDisabled; }
    [[nodiscard]] bool isHighResTimerActive() const noexcept { return mTimerPeriodActive; }

private:
    bool mPowerThrottlingDisabled { false };
    bool mTimerPeriodActive { false };

#if JUCE_WINDOWS
    struct Rb26ProcessPowerThrottlingState {
        ULONG Version;
        ULONG ControlMask;
        ULONG StateMask;
    };

    void disableProcessPowerThrottling() noexcept {
        HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
        if (kernel32 == nullptr) {
            return;
        }

        using SetProcessInformationFn = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);
        // Avoid cast-function-type warnings across compilers via void* intermediate
        const auto rawProc = reinterpret_cast<void*>(::GetProcAddress(kernel32, "SetProcessInformation"));
        if (rawProc == nullptr) {
            return;
        }
        SetProcessInformationFn setProcInfo = nullptr;
        std::memcpy(&setProcInfo, &rawProc, sizeof(setProcInfo));

        constexpr int kProcessPowerThrottlingInfoClass = 4; // ProcessPowerThrottling
        constexpr ULONG kCurrentVersion = 1;
        constexpr ULONG kExecutionSpeed = 0x1;              // PROCESS_POWER_THROTTLING_EXECUTION_SPEED (Win10 1709+)
        constexpr ULONG kIgnoreTimerResolution = 0x4;       // PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION (Win10 2004+ / Win11)

        // First attempt disabling both Execution Speed (EcoQoS) and Timer Resolution throttling.
        Rb26ProcessPowerThrottlingState state {};
        state.Version = kCurrentVersion;
        state.ControlMask = kExecutionSpeed | kIgnoreTimerResolution;
        state.StateMask = 0;

        if (setProcInfo(::GetCurrentProcess(), kProcessPowerThrottlingInfoClass, &state, sizeof(state)) != FALSE) {
            mPowerThrottlingDisabled = true;
            return;
        }

        // Fallback for earlier Windows 10 builds (1709-1909) that reject unknown ControlMask bit 0x4.
        state.ControlMask = kExecutionSpeed;
        state.StateMask = 0;
        if (setProcInfo(::GetCurrentProcess(), kProcessPowerThrottlingInfoClass, &state, sizeof(state)) != FALSE) {
            mPowerThrottlingDisabled = true;
        }
    }

    static void elevateProcessPriorityIfNormal() noexcept {
        const HANDLE proc = ::GetCurrentProcess();
        if (::GetPriorityClass(proc) == NORMAL_PRIORITY_CLASS) {
            (void)::SetPriorityClass(proc, ABOVE_NORMAL_PRIORITY_CLASS);
        }
    }
#endif
};

// ============================================================================
// 2. Windows S3 Sleep & S0 Modern Standby Suspend/Resume Listener
// ============================================================================
// Listens to both broadcast WM_POWERBROADCAST messages on a hidden top-level
// WS_POPUP HWND and explicit PowerRegisterSuspendResumeNotification callbacks
// so Modern Standby (S0 Low Power Idle) laptop lid close/reopen cycles reliably
// notify the Standalone audio device recovery state machine.
class WindowsPowerSuspendNotifier {
public:
    using Callback = std::function<void()>;

    WindowsPowerSuspendNotifier(Callback onSuspend, Callback onResume)
        : mOnSuspend(std::move(onSuspend)),
          mOnResume(std::move(onResume)) {
#if JUCE_WINDOWS
        createHiddenPowerWindow();
#else
        (void)mOnSuspend;
        (void)mOnResume;
#endif
    }

    ~WindowsPowerSuspendNotifier() noexcept {
#if JUCE_WINDOWS
        destroyHiddenPowerWindow();
#endif
    }

    WindowsPowerSuspendNotifier(const WindowsPowerSuspendNotifier&) = delete;
    WindowsPowerSuspendNotifier& operator=(const WindowsPowerSuspendNotifier&) = delete;

private:
    Callback mOnSuspend;
    Callback mOnResume;

#if JUCE_WINDOWS
    HWND mHwnd { nullptr };
    HMODULE mPowrProfLib { nullptr };
    PVOID mSuspendResumeRegHandle { nullptr };
    ATOM mWindowClassAtom { 0 };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
        if (msg == WM_NCCREATE) {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }

        auto* self = reinterpret_cast<WindowsPowerSuspendNotifier*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_POWERBROADCAST && self != nullptr) {
            if (wParam == PBT_APMSUSPEND) {
                if (self->mOnSuspend) {
                    self->mOnSuspend();
                }
                return TRUE;
            }
            if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                if (self->mOnResume) {
                    self->mOnResume();
                }
                return TRUE;
            }
        }

        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void createHiddenPowerWindow() noexcept {
        const HINSTANCE hInst = ::GetModuleHandleW(nullptr);
        const juce::String clsName = "BraunRb26PowerNotify_" + juce::String::toHexString(reinterpret_cast<uintptr_t>(this));

        WNDCLASSEXW wc {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = &WindowsPowerSuspendNotifier::wndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = clsName.toWideCharPointer();

        mWindowClassAtom = ::RegisterClassExW(&wc);
        if (mWindowClassAtom == 0) {
            return;
        }

        // Top-level hidden WS_POPUP (NOT HWND_MESSAGE) so OS-wide WM_POWERBROADCAST reaches us
        mHwnd = ::CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            clsName.toWideCharPointer(),
            L"BraunRb26PowerNotify",
            WS_POPUP,
            0, 0, 0, 0,
            nullptr, nullptr, hInst, this);

        if (mHwnd == nullptr) {
            return;
        }

        // Also register for Windows 8/10/11 S0 Modern Standby suspend/resume notifications
        mPowrProfLib = ::LoadLibraryW(L"powrprof.dll");
        if (mPowrProfLib != nullptr) {
            using PowerRegFn = DWORD(WINAPI*)(ULONG, HANDLE, PVOID*);
            const auto rawReg = reinterpret_cast<void*>(::GetProcAddress(mPowrProfLib, "PowerRegisterSuspendResumeNotification"));
            if (rawReg != nullptr) {
                PowerRegFn regFn = nullptr;
                std::memcpy(&regFn, &rawReg, sizeof(regFn));
                constexpr ULONG kDeviceNotifyWindowHandle = 0; // DEVICE_NOTIFY_WINDOW_HANDLE
                (void)regFn(kDeviceNotifyWindowHandle, mHwnd, &mSuspendResumeRegHandle);
            }
        }
    }

    void destroyHiddenPowerWindow() noexcept {
        if (mSuspendResumeRegHandle != nullptr && mPowrProfLib != nullptr) {
            using PowerUnregFn = DWORD(WINAPI*)(PVOID);
            const auto rawUnreg = reinterpret_cast<void*>(::GetProcAddress(mPowrProfLib, "PowerUnregisterSuspendResumeNotification"));
            if (rawUnreg != nullptr) {
                PowerUnregFn unregFn = nullptr;
                std::memcpy(&unregFn, &rawUnreg, sizeof(unregFn));
                (void)unregFn(mSuspendResumeRegHandle);
            }
            mSuspendResumeRegHandle = nullptr;
        }

        if (mHwnd != nullptr) {
            ::SetWindowLongPtrW(mHwnd, GWLP_USERDATA, 0);
            ::DestroyWindow(mHwnd);
            mHwnd = nullptr;
        }

        if (mWindowClassAtom != 0) {
            ::UnregisterClassW(reinterpret_cast<LPCWSTR>(static_cast<uintptr_t>(mWindowClassAtom)), ::GetModuleHandleW(nullptr));
            mWindowClassAtom = 0;
        }

        if (mPowrProfLib != nullptr) {
            ::FreeLibrary(mPowrProfLib);
            mPowrProfLib = nullptr;
        }
    }
#endif
};

// ============================================================================
// 3. Lock-Free SPSC Elastic Drift-Compensating Ring Buffer (4-Point Hermite ASRC)
// ============================================================================
// Bridges independent capture (e.g. VB-Cable QPC software clock) and render
// (e.g. Realtek / USB DAC hardware crystal) threads with:
//   - Zero real-time heap allocations and zero locks (strict 64-bit SPSC atomics)
//   - Pre-primed bidirectional cushion so neither positive nor negative drift
//     can immediately underrun or overflow
//   - Overdamped 2nd-order PI controller (wn = 0.25 rad/s, zeta = 1.2) driving
//     4-point 3rd-order Hermite interpolation for click-free, transparent drift tracking
//   - Smooth raised-cosine crossfade recovery on extreme OS sleep/stall bursts
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

class ElasticDriftRingBuffer {
public:
    static constexpr int kMaxChannels = 16;
    static constexpr uint64_t kCapacity = 32768; // ~682 ms at 48 kHz, power of two
    static constexpr uint64_t kMask = kCapacity - 1;
    static constexpr int kCrossfadeFrames = 128;
    static constexpr double kPi = 3.14159265358979323846;

    ElasticDriftRingBuffer() {
        for (auto& chBuf : mStorage) {
            chBuf.assign(static_cast<size_t>(kCapacity), 0.0f);
        }
        reset();
    }

    void prepare(int numChannels,
                 int producerBlockSize,
                 int consumerBlockSize,
                 double producerSampleRate,
                 double consumerSampleRate) {
        mNumChannels = std::clamp(numChannels, 0, kMaxChannels);
        mProducerRate = (producerSampleRate > 1000.0) ? producerSampleRate : 48000.0;
        mConsumerRate = (consumerSampleRate > 1000.0) ? consumerSampleRate : 48000.0;
        mNominalRatio = std::clamp(mProducerRate / mConsumerRate, 0.125, 8.0);

        const int safeProdBlock = std::clamp(producerBlockSize, 32, 8192);
        const int safeConsBlock = std::clamp(consumerBlockSize, 32, 8192);
        const int consBlockInProdFrames = static_cast<int>(std::ceil(static_cast<double>(safeConsBlock) * mNominalRatio));
        const int jitterMarginFrames = static_cast<int>(std::ceil(0.010 * mProducerRate)); // 10 ms OS scheduling cushion

        mProducerBlockFrames = safeProdBlock;
        mPacketQuantizationBand = static_cast<double>(std::max(safeProdBlock, consBlockInProdFrames)) * 1.15;
        mTargetFill = std::clamp(2 * std::max(safeProdBlock, consBlockInProdFrames) + consBlockInProdFrames + jitterMarginFrames,
                                 512,
                                 static_cast<int>(kCapacity / 4));
        mHighWaterMark = std::min<int>(static_cast<int>(kCapacity) - 256,
                                       std::max(mTargetFill * 3, mTargetFill + 4096));

        for (auto& chBuf : mStorage) {
            if (chBuf.size() != static_cast<size_t>(kCapacity)) {
                chBuf.assign(static_cast<size_t>(kCapacity), 0.0f);
            }
        }

        reset();
    }

    // Must only be called before producer/consumer threads start (or after both stop).
    void reset() noexcept {
        for (int ch = 0; ch < mNumChannels; ++ch) {
            std::fill(mStorage[static_cast<size_t>(ch)].begin(),
                      mStorage[static_cast<size_t>(ch)].end(),
                      0.0f);
        }
        mLastOutSample.fill(0.0f);
        mFadeStartSample.fill(0.0f);

        // Start read head at index 2 and pre-fill (mTargetFill - P/2) frames so that after
        // producer pushes +P frames, the pre-pull fill sits symmetrically centered on mTargetFill!
        constexpr uint64_t kInitialReadOffset = 2;
        const int initialPreFill = std::max(256, mTargetFill - (mProducerBlockFrames / 2));
        mReadPos.store(kInitialReadOffset, std::memory_order_relaxed);
        mWritePos.store(kInitialReadOffset + static_cast<uint64_t>(initialPreFill), std::memory_order_relaxed);

        mFrac = 0.0;
        mSmoothedErr = 0.0;
        mIntegral = 0.0;
        mCurrentRatio.store(mNominalRatio, std::memory_order_relaxed);
        mEstimatedFill.store(static_cast<double>(mTargetFill), std::memory_order_relaxed);

        mWaitingForPrefill = false;
        mFadingInFromLast = false;
        mFadeInPos = 0;
        mCrossfading = false;
        mCrossfadePos = 0;
        mOldReadPos = kInitialReadOffset;
        mOldFrac = 0.0;

        mProducerOverflowed.store(false, std::memory_order_relaxed);
        mPushBlocks.store(0, std::memory_order_relaxed);
        mPullBlocks.store(0, std::memory_order_relaxed);
        mUnderrunCount.store(0, std::memory_order_relaxed);
        mOverflowCount.store(0, std::memory_order_relaxed);
        mRecenterCount.store(0, std::memory_order_relaxed);
    }

    // Producer thread (capture callback): lock-free, 0 allocations.
    void push(const float* const* inputChannels, int numChannels, int numSamples) noexcept {
        if (numSamples <= 0 || mNumChannels <= 0) {
            return;
        }

        const uint64_t w = mWritePos.load(std::memory_order_relaxed);
        const uint64_t r = mReadPos.load(std::memory_order_acquire);
        const uint64_t avail = (w >= r) ? (w - r) : 0;
        constexpr uint64_t kMaxSafeFill = kCapacity - 16; // Guard margin for [r-1 .. r+2]

        if (avail >= kMaxSafeFill) {
            mProducerOverflowed.store(true, std::memory_order_release);
            mOverflowCount.fetch_add(1, std::memory_order_relaxed);
            mPushBlocks.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const uint64_t space = kMaxSafeFill - avail;
        const int framesToWrite = static_cast<int>(std::min<uint64_t>(static_cast<uint64_t>(numSamples), space));
        if (framesToWrite < numSamples) {
            mProducerOverflowed.store(true, std::memory_order_release);
            mOverflowCount.fetch_add(1, std::memory_order_relaxed);
        }

        for (int ch = 0; ch < mNumChannels; ++ch) {
            float* dst = mStorage[static_cast<size_t>(ch)].data();
            const float* src = (inputChannels != nullptr && ch < numChannels) ? inputChannels[ch] : nullptr;
            if (src != nullptr) {
                for (int i = 0; i < framesToWrite; ++i) {
                    dst[static_cast<size_t>((w + static_cast<uint64_t>(i)) & kMask)] = sanitizeSample(src[i]);
                }
            } else {
                for (int i = 0; i < framesToWrite; ++i) {
                    dst[static_cast<size_t>((w + static_cast<uint64_t>(i)) & kMask)] = 0.0f;
                }
            }
        }

        mWritePos.store(w + static_cast<uint64_t>(framesToWrite), std::memory_order_release);
        mPushBlocks.fetch_add(1, std::memory_order_relaxed);
    }

    // Consumer thread (render callback): lock-free, 0 allocations.
    void pull(float* const* outputChannels, int numChannels, int numSamples) noexcept {
        if (outputChannels == nullptr || numSamples <= 0) {
            return;
        }

        const int activeOutChans = std::min(numChannels, mNumChannels);
        for (int ch = activeOutChans; ch < numChannels; ++ch) {
            if (outputChannels[ch] != nullptr) {
                std::fill(outputChannels[ch], outputChannels[ch] + numSamples, 0.0f);
            }
        }

        if (activeOutChans <= 0) {
            return;
        }

        const uint64_t w = mWritePos.load(std::memory_order_acquire);
        uint64_t r = mReadPos.load(std::memory_order_relaxed);

        // 1. High-water / overflow self-healing recenter with click-free raised-cosine crossfade
        const bool producerOverflowed = mProducerOverflowed.exchange(false, std::memory_order_acq_rel);
        const uint64_t initialAvailInt = (w >= r) ? (w - r) : 0;
        if ((producerOverflowed || initialAvailInt > static_cast<uint64_t>(mHighWaterMark))
            && w >= static_cast<uint64_t>(mTargetFill + 4)) {
            const uint64_t newR = w - static_cast<uint64_t>(mTargetFill);
            if (initialAvailInt >= 4 && r >= 1 && (w - r) < (kCapacity - 8)) {
                mOldReadPos = r;
                mOldFrac = mFrac;
                mCrossfading = true;
                mCrossfadePos = 0;
            } else {
                mFadeStartSample = mLastOutSample;
                mFadingInFromLast = true;
                mFadeInPos = 0;
                mCrossfading = false;
            }

            r = newR;
            mFrac = 0.0;
            mSmoothedErr = 0.0;
            mIntegral = 0.0;
            mWaitingForPrefill = false;
            mRecenterCount.fetch_add(1, std::memory_order_relaxed);
        }

        // 2. Underrun prefill wait & smooth recovery
        if (mWaitingForPrefill) {
            const uint64_t availNow = (w >= r) ? (w - r) : 0;
            if (availNow < static_cast<uint64_t>(mTargetFill)) {
                decayToZeroBlock(outputChannels, activeOutChans, 0, numSamples);
                mEstimatedFill.store(static_cast<double>(availNow), std::memory_order_relaxed);
                mPullBlocks.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            mWaitingForPrefill = false;
            mSmoothedErr = 0.0;
            mIntegral = 0.0;
            mFadeStartSample = mLastOutSample;
            mFadingInFromLast = true;
            mFadeInPos = 0;
        }

        // 3. Overdamped gain-scheduled 2nd-order PI drift controller:
        //    - Inside the +/- 1-packet arrival quantization sawtooth (mPacketQuantizationBand),
        //      uses ultra-low bandwidth (wn = 0.025 rad/s, zeta = 1.15) so discrete WASAPI/VB-Cable
        //      packet beats never induce audible FM/pitch modulation (< 0.4 cents worst-case).
        //    - Outside the packet band, smoothly increases proportional pull (up to 0.35 / fs)
        //      to rapidly arrest large startup or severe clock-skew excursions without underrun/overflow.
        const double exactAvail = static_cast<double>((w >= r) ? (w - r) : 0) - mFrac;
        const double rawErr = exactAvail - static_cast<double>(mTargetFill);
        const double blockDt = static_cast<double>(numSamples) / mConsumerRate;

        const double errAlpha = 1.0 - std::exp(-blockDt / 1.50);
        mSmoothedErr += errAlpha * (rawErr - mSmoothedErr);

        constexpr double kWnSmall = 0.025; // 40s fine-lock horizon (zeta >= 1.15 everywhere)
        constexpr double kZetaSmall = 1.15;
        const double kpSmall = (2.0 * kZetaSmall * kWnSmall) / mProducerRate; // ~1.20e-6 @ 48 kHz
        const double kpLarge = 0.35 / mProducerRate;                          // ~7.29e-6 @ 48 kHz
        const double ki = (kWnSmall * kWnSmall) / mProducerRate;

        const double band = std::max(64.0, mPacketQuantizationBand);
        const double excessRatio = std::clamp((std::abs(mSmoothedErr) - band) / band, 0.0, 1.0);
        const double kpEff = kpSmall + (kpLarge - kpSmall) * excessRatio;

        mIntegral = std::clamp(mIntegral + ki * mSmoothedErr * blockDt, -0.010, 0.010);
        const double trim = std::clamp(kpEff * mSmoothedErr + mIntegral, -0.015, 0.015);
        const double stepRatio = mNominalRatio * (1.0 + trim);
        mCurrentRatio.store(stepRatio, std::memory_order_relaxed);

        // 4. Sample-by-sample 4-point Hermite fractional read
        for (int n = 0; n < numSamples; ++n) {
            if (w < r + 4 || r < 1) {
                // Ran dry: smoothly ramp last valid sample toward 0 instead of hard zeromem pop
                mWaitingForPrefill = true;
                mCrossfading = false;
                mFadingInFromLast = false;
                mUnderrunCount.fetch_add(1, std::memory_order_relaxed);
                decayToZeroBlock(outputChannels, activeOutChans, n, numSamples);
                break;
            }

            const float frac = static_cast<float>(mFrac);
            float crossfadeAlpha = 1.0f;
            bool doDualCrossfade = false;
            if (mCrossfading) {
                if (w >= mOldReadPos + 4 && mOldReadPos >= 1) {
                    crossfadeAlpha = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * static_cast<double>(mCrossfadePos + 1) / static_cast<double>(kCrossfadeFrames))));
                    doDualCrossfade = true;
                } else {
                    mCrossfading = false;
                }
            }

            float fadeInAlpha = 1.0f;
            const bool doFadeIn = mFadingInFromLast;
            if (doFadeIn) {
                fadeInAlpha = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * static_cast<double>(mFadeInPos + 1) / static_cast<double>(kCrossfadeFrames))));
            }

            const float oldFrac = static_cast<float>(mOldFrac);
            for (int ch = 0; ch < activeOutChans; ++ch) {
                const float* src = mStorage[static_cast<size_t>(ch)].data();
                float sample = readHermite4P3O(src, r, frac);

                if (doDualCrossfade) {
                    const float oldSample = readHermite4P3O(src, mOldReadPos, oldFrac);
                    sample = (1.0f - crossfadeAlpha) * oldSample + crossfadeAlpha * sample;
                } else if (doFadeIn) {
                    sample = (1.0f - fadeInAlpha) * mFadeStartSample[static_cast<size_t>(ch)] + fadeInAlpha * sample;
                }

                sample = sanitizeSample(sample);
                mLastOutSample[static_cast<size_t>(ch)] = sample;
                if (outputChannels[ch] != nullptr) {
                    outputChannels[ch][n] = sample;
                }
            }

            mFrac += stepRatio;
            const uint64_t adv = static_cast<uint64_t>(mFrac);
            r += adv;
            mFrac -= static_cast<double>(adv);

            if (mCrossfading) {
                mOldFrac += stepRatio;
                const uint64_t oldAdv = static_cast<uint64_t>(mOldFrac);
                mOldReadPos += oldAdv;
                mOldFrac -= static_cast<double>(oldAdv);
                if (++mCrossfadePos >= kCrossfadeFrames) {
                    mCrossfading = false;
                }
            }

            if (mFadingInFromLast) {
                if (++mFadeInPos >= kCrossfadeFrames) {
                    mFadingInFromLast = false;
                }
            }
        }

        mReadPos.store(r, std::memory_order_release);
        mEstimatedFill.store(static_cast<double>((w >= r) ? (w - r) : 0) - mFrac, std::memory_order_relaxed);
        mPullBlocks.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] int getTargetFillFrames() const noexcept { return mTargetFill; }
    [[nodiscard]] double getCurrentRatio() const noexcept { return mCurrentRatio.load(std::memory_order_relaxed); }
    [[nodiscard]] double getEstimatedFillFrames() const noexcept { return mEstimatedFill.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPushBlockCount() const noexcept { return mPushBlocks.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getPullBlockCount() const noexcept { return mPullBlocks.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getUnderrunCount() const noexcept { return mUnderrunCount.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getOverflowCount() const noexcept { return mOverflowCount.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t getRecenterCount() const noexcept { return mRecenterCount.load(std::memory_order_relaxed); }

private:
    std::array<std::vector<float>, kMaxChannels> mStorage {};
    std::array<float, kMaxChannels> mLastOutSample {};
    std::array<float, kMaxChannels> mFadeStartSample {};

    int mNumChannels { 2 };
    int mProducerBlockFrames { 480 };
    double mProducerRate { 48000.0 };
    double mConsumerRate { 48000.0 };
    double mNominalRatio { 1.0 };
    double mPacketQuantizationBand { 320.0 };
    int mTargetFill { 1920 };
    int mHighWaterMark { 5952 };

    alignas(64) std::atomic<uint64_t> mWritePos { 0 };
    alignas(64) std::atomic<uint64_t> mReadPos { 0 };
    std::atomic<bool> mProducerOverflowed { false };

    // Consumer-owned state
    double mFrac { 0.0 };
    double mSmoothedErr { 0.0 };
    double mIntegral { 0.0 };
    bool mWaitingForPrefill { false };
    bool mFadingInFromLast { false };
    int mFadeInPos { 0 };
    bool mCrossfading { false };
    int mCrossfadePos { 0 };
    uint64_t mOldReadPos { 2 };
    double mOldFrac { 0.0 };

    // Lock-free telemetry
    std::atomic<double> mCurrentRatio { 1.0 };
    std::atomic<double> mEstimatedFill { 0.0 };
    std::atomic<uint64_t> mPushBlocks { 0 };
    std::atomic<uint64_t> mPullBlocks { 0 };
    std::atomic<uint64_t> mUnderrunCount { 0 };
    std::atomic<uint64_t> mOverflowCount { 0 };
    std::atomic<uint64_t> mRecenterCount { 0 };

    [[nodiscard]] static float sanitizeSample(float x) noexcept {
        if (!std::isfinite(x)) {
            return 0.0f;
        }
        return (std::abs(x) < 1.0e-20f) ? 0.0f : x;
    }

    [[nodiscard]] static float readHermite4P3O(const float* buf, uint64_t idx, float t) noexcept {
        const float ym1 = buf[static_cast<size_t>((idx - 1) & kMask)];
        const float y0  = buf[static_cast<size_t>(idx & kMask)];
        const float y1  = buf[static_cast<size_t>((idx + 1) & kMask)];
        const float y2  = buf[static_cast<size_t>((idx + 2) & kMask)];

        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    void decayToZeroBlock(float* const* outputChannels, int activeOutChans, int startSample, int endSample) noexcept {
        for (int n = startSample; n < endSample; ++n) {
            for (int ch = 0; ch < activeOutChans; ++ch) {
                float v = mLastOutSample[static_cast<size_t>(ch)] * 0.94f;
                if (std::abs(v) < 1.0e-7f) {
                    v = 0.0f;
                }
                mLastOutSample[static_cast<size_t>(ch)] = v;
                if (outputChannels[ch] != nullptr) {
                    outputChannels[ch][n] = v;
                }
            }
        }
    }
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ============================================================================
// 4. Duplex Drift-Compensated AudioIODevice & AudioIODeviceType
// ============================================================================
// Splits separate-clock Windows audio endpoints (e.g. VB-Cable Input + Realtek/USB Output)
// into two independent single-direction AudioIODevice threads bridged by
// ElasticDriftRingBuffer, while monitoring for post-sleep WASAPI endpoint invalidation
// runaway loops and silent capture-thread timeouts.
class DuplexDriftCompensatedAudioIODevice final : public juce::AudioIODevice {
public:
    DuplexDriftCompensatedAudioIODevice(const juce::String& deviceName,
                                        const juce::String& typeName,
                                        juce::AudioIODeviceType& parentType,
                                        std::unique_ptr<juce::AudioIODevice> outputDeviceIn,
                                        std::unique_ptr<juce::AudioIODevice> inputDeviceIn,
                                        const juce::String& outputDeviceNameIn,
                                        const juce::String& inputDeviceNameIn)
        : juce::AudioIODevice(deviceName, typeName),
          mParentType(parentType),
          mOutputDev(std::move(outputDeviceIn)),
          mInputDev(std::move(inputDeviceIn)),
          mOutputDeviceName(outputDeviceNameIn),
          mInputDeviceName(inputDeviceNameIn),
          mInputCallback(*this),
          mOutputCallback(*this) {}

    ~DuplexDriftCompensatedAudioIODevice() override {
        close();
    }

    juce::StringArray getOutputChannelNames() override {
        return mOutputDev != nullptr ? mOutputDev->getOutputChannelNames() : juce::StringArray{};
    }

    juce::StringArray getInputChannelNames() override {
        return mInputDev != nullptr ? mInputDev->getInputChannelNames() : juce::StringArray{};
    }

    juce::Array<double> getAvailableSampleRates() override {
        if (mOutputDev != nullptr && mInputDev != nullptr) {
            const auto outRates = mOutputDev->getAvailableSampleRates();
            const auto inRates = mInputDev->getAvailableSampleRates();
            juce::Array<double> common;
            for (const auto r : outRates) {
                if (inRates.contains(r)) {
                    common.add(r);
                }
            }
            // If common rates exist, prefer 1:1 nominal rate; otherwise ElasticDriftRingBuffer
            // resamples input's native rate onto output's clock seamlessly!
            return common.isEmpty() ? outRates : common;
        }
        if (mOutputDev != nullptr) {
            return mOutputDev->getAvailableSampleRates();
        }
        if (mInputDev != nullptr) {
            return mInputDev->getAvailableSampleRates();
        }
        return {};
    }

    juce::Array<int> getAvailableBufferSizes() override {
        if (mOutputDev != nullptr) {
            return mOutputDev->getAvailableBufferSizes();
        }
        if (mInputDev != nullptr) {
            return mInputDev->getAvailableBufferSizes();
        }
        return {};
    }

    int getDefaultBufferSize() override {
        if (mOutputDev != nullptr && mInputDev != nullptr) {
            const int outDef = mOutputDev->getDefaultBufferSize();
            const int inDef = mInputDev->getDefaultBufferSize();
            const int combined = std::max(outDef, inDef);
            const auto outSizes = mOutputDev->getAvailableBufferSizes();
            return outSizes.contains(combined) ? combined : outDef;
        }
        if (mOutputDev != nullptr) {
            return mOutputDev->getDefaultBufferSize();
        }
        if (mInputDev != nullptr) {
            return mInputDev->getDefaultBufferSize();
        }
        return 512;
    }

    juce::String open(const juce::BigInteger& inputChannels,
                      const juce::BigInteger& outputChannels,
                      double sampleRate,
                      int bufferSizeSamples) override {
        close();
        mLastError.clear();
        mStreamFaulted.store(false, std::memory_order_relaxed);
        mRateWindowFrames = 0;
        mRateWindowBlocks = 0;
        mRateWindowStartTick = 0;

        mRequestedInputChannels = inputChannels;
        mRequestedOutputChannels = outputChannels;
        mRequestedBufferSize = bufferSizeSamples;

        const bool wantOutput = (mOutputDev != nullptr && !outputChannels.isZero());
        const bool wantInput  = (mInputDev != nullptr && !inputChannels.isZero());

        if (wantOutput) {
            mLastError = mOutputDev->open({}, outputChannels, sampleRate, bufferSizeSamples);
            if (mLastError.isNotEmpty()) {
                close();
                return mLastError;
            }
            mCurrentSampleRate = mOutputDev->getCurrentSampleRate();
            mCurrentBufferSize = mOutputDev->getCurrentBufferSizeSamples();
        } else {
            mCurrentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
            mCurrentBufferSize = bufferSizeSamples > 0 ? bufferSizeSamples : getDefaultBufferSize();
        }

        double actualInputRate = mCurrentSampleRate;
        int actualInputBufSize = mCurrentBufferSize;

        if (wantInput) {
            juce::String inErr = mInputDev->open(inputChannels, {}, mCurrentSampleRate, bufferSizeSamples);
            if (inErr.isNotEmpty() && wantOutput) {
                // Fallback for Low-Latency / Exclusive endpoints locked to a distinct native rate/period (e.g. VB-Cable 44.1k vs Speakers 48k)
                inErr = mInputDev->open(inputChannels, {}, 0.0, 0);
            }
            if (inErr.isNotEmpty()) {
                mLastError = inErr;
                close();
                return mLastError;
            }
            actualInputRate = mInputDev->getCurrentSampleRate();
            actualInputBufSize = mInputDev->getCurrentBufferSizeSamples();
            if (!wantOutput) {
                mCurrentSampleRate = actualInputRate;
                mCurrentBufferSize = actualInputBufSize;
            }
        }

        mNumActiveInputs = wantInput ? mInputDev->getActiveInputChannels().countNumberOfSetBits() : 0;

        const int scratchChans = std::max(1, mNumActiveInputs);
        const int scratchSamples = std::max(16384, mCurrentBufferSize * 8);
        mScratchInputBuffer.setSize(scratchChans, scratchSamples, false, true, false);
        mScratchInputPtrs.assign(static_cast<size_t>(scratchChans), nullptr);
        for (int ch = 0; ch < scratchChans; ++ch) {
            mScratchInputPtrs[static_cast<size_t>(ch)] = mScratchInputBuffer.getWritePointer(ch);
        }

        if (wantInput && wantOutput && mNumActiveInputs > 0) {
            mRingBuffer.prepare(mNumActiveInputs,
                                actualInputBufSize,
                                mCurrentBufferSize,
                                actualInputRate,
                                mCurrentSampleRate);
        }

        mInputOpened = wantInput;
        mOutputOpened = wantOutput;
        mIsOpen = true;
        return {};
    }

    void close() override {
        stop();
        if (mInputDev != nullptr) {
            mInputDev->close();
        }
        if (mOutputDev != nullptr) {
            mOutputDev->close();
        }
        mInputOpened = false;
        mOutputOpened = false;
        mIsOpen = false;
    }

    bool isOpen() override {
        if (!mIsOpen) {
            return false;
        }
        if (mOutputOpened && (mOutputDev == nullptr || !mOutputDev->isOpen())) {
            return false;
        }
        if (!mOutputOpened && mInputOpened && (mInputDev == nullptr || !mInputDev->isOpen())) {
            return false;
        }
        return true;
    }

    void start(juce::AudioIODeviceCallback* call) override {
        if (!mIsOpen || call == nullptr || mIsPlaying.load(std::memory_order_acquire)) {
            return;
        }

        if (mInputOpened && mOutputOpened && mNumActiveInputs > 0) {
            mRingBuffer.reset();
        }

        mStreamFaulted.store(false, std::memory_order_relaxed);
        mRateWindowFrames = 0;
        mRateWindowBlocks = 0;
        mRateWindowStartTick = 0;
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        mLastOutputCallbackMs.store(nowMs, std::memory_order_relaxed);
        mLastInputCallbackMs.store(nowMs, std::memory_order_relaxed);

        call->audioDeviceAboutToStart(this);

        {
            const juce::ScopedLock sl(mCallbackLock);
            mUserCallback = call;
            mIsPlaying.store(true, std::memory_order_release);
        }

        // Start capture BEFORE render so the elastic ring buffer begins filling immediately.
        if (mInputOpened && mOutputOpened) {
            mInputDev->start(&mInputCallback);
            mOutputDev->start(&mOutputCallback);
        } else if (mOutputOpened) {
            mOutputDev->start(&mOutputCallback);
        } else if (mInputOpened) {
            mInputDev->start(&mInputCallback);
        }
    }

    void stop() override {
        if (!mIsPlaying.load(std::memory_order_acquire)) {
            return;
        }

        juce::AudioIODeviceCallback* prevCallback = nullptr;
        {
            const juce::ScopedLock sl(mCallbackLock);
            mIsPlaying.store(false, std::memory_order_release);
            prevCallback = mUserCallback;
            mUserCallback = nullptr;
        }

        if (mOutputOpened && mOutputDev != nullptr) {
            mOutputDev->stop();
        }
        if (mInputOpened && mInputDev != nullptr) {
            mInputDev->stop();
        }

        if (prevCallback != nullptr) {
            prevCallback->audioDeviceStopped();
        }
    }

    bool isPlaying() override {
        return mIsPlaying.load(std::memory_order_acquire) && isOpen();
    }

    juce::String getLastError() override { return mLastError; }
    int getCurrentBufferSizeSamples() override { return mCurrentBufferSize; }
    double getCurrentSampleRate() override { return mCurrentSampleRate; }
    int getCurrentBitDepth() override {
        if (mOutputDev != nullptr) {
            return mOutputDev->getCurrentBitDepth();
        }
        if (mInputDev != nullptr) {
            return mInputDev->getCurrentBitDepth();
        }
        return 32;
    }

    juce::BigInteger getActiveOutputChannels() const override {
        return (mOutputOpened && mOutputDev != nullptr) ? mOutputDev->getActiveOutputChannels() : juce::BigInteger{};
    }

    juce::BigInteger getActiveInputChannels() const override {
        return (mInputOpened && mInputDev != nullptr) ? mInputDev->getActiveInputChannels() : juce::BigInteger{};
    }

    int getOutputLatencyInSamples() override {
        return (mOutputOpened && mOutputDev != nullptr) ? mOutputDev->getOutputLatencyInSamples() : 0;
    }

    int getInputLatencyInSamples() override {
        const int baseIn = (mInputOpened && mInputDev != nullptr) ? mInputDev->getInputLatencyInSamples() : 0;
        const int ringCushion = (mInputOpened && mOutputOpened && mNumActiveInputs > 0) ? mRingBuffer.getTargetFillFrames() : 0;
        return baseIn + ringCushion;
    }

    bool hasControlPanel() const override {
        return (mOutputDev != nullptr && mOutputDev->hasControlPanel())
            || (mInputDev != nullptr && mInputDev->hasControlPanel());
    }

    bool showControlPanel() override {
        if (mOutputDev != nullptr && mOutputDev->hasControlPanel()) {
            return mOutputDev->showControlPanel();
        }
        if (mInputDev != nullptr && mInputDev->hasControlPanel()) {
            return mInputDev->showControlPanel();
        }
        return false;
    }

    int getXRunCount() const noexcept override {
        const int inX = (mInputDev != nullptr) ? std::max(0, mInputDev->getXRunCount()) : 0;
        const int outX = (mOutputDev != nullptr) ? std::max(0, mOutputDev->getXRunCount()) : 0;
        return inX + outX + static_cast<int>(mRingBuffer.getUnderrunCount() + mRingBuffer.getRecenterCount());
    }

    std::optional<juce::BigInteger> getDefaultOutputChannels() const override {
        return mOutputDev != nullptr ? mOutputDev->getDefaultOutputChannels() : std::nullopt;
    }

    std::optional<juce::BigInteger> getDefaultInputChannels() const override {
        return mInputDev != nullptr ? mInputDev->getDefaultInputChannels() : std::nullopt;
    }

    // --- Health & Non-Disruptive Input Recovery Accessors ---
    [[nodiscard]] juce::AudioIODevice* getInnerOutputDevice() const noexcept { return mOutputDev.get(); }
    [[nodiscard]] juce::AudioIODevice* getInnerInputDevice() const noexcept { return mInputDev.get(); }
    [[nodiscard]] const ElasticDriftRingBuffer& getRingBuffer() const noexcept { return mRingBuffer; }

    [[nodiscard]] bool hasFatalStreamFault() const noexcept {
        if (mStreamFaulted.load(std::memory_order_acquire)) {
            return true;
        }
        if (!mIsOpen || !mIsPlaying.load(std::memory_order_acquire)) {
            return false;
        }
        if (mOutputOpened && (mOutputDev == nullptr || !mOutputDev->isOpen())) {
            return true;
        }
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (mOutputOpened && (nowMs - mLastOutputCallbackMs.load(std::memory_order_relaxed)) > 2500.0) {
            return true;
        }
        if (!mOutputOpened && mInputOpened && (nowMs - mLastInputCallbackMs.load(std::memory_order_relaxed)) > 2500.0) {
            return true;
        }
        return false;
    }

    // Hot-restarts ONLY the capture endpoint on the message thread if JUCE's WASAPI
    // input-only WaitForSingleObject(1000 ms) exited during an idle/sleep pause,
    // without interrupting the active output stream!
    bool checkAndRecoverInputStreamOnMessageThread() {
        if (!mIsOpen || !mIsPlaying.load(std::memory_order_acquire) || !mInputOpened || !mOutputOpened) {
            return false;
        }

        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const bool inputDead = (mInputDev == nullptr)
                            || !mInputDev->isOpen()
                            || !mInputDev->isPlaying()
                            || ((nowMs - mLastInputCallbackMs.load(std::memory_order_relaxed)) > 2200.0);
        if (!inputDead) {
            return false;
        }

        if (mInputDev != nullptr) {
            mInputDev->stop();
            mInputDev->close();
            mInputDev.reset();
        }

        mParentType.scanForDevices();
        mInputDev.reset(mParentType.createDevice({}, mInputDeviceName));
        if (mInputDev == nullptr) {
            return false;
        }

        juce::String err = mInputDev->open(mRequestedInputChannels, {}, mCurrentSampleRate, mRequestedBufferSize);
        if (err.isNotEmpty()) {
            err = mInputDev->open(mRequestedInputChannels, {}, 0.0, 0);
        }
        if (err.isNotEmpty()) {
            mInputDev->close();
            return false;
        }

        mLastInputCallbackMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);
        mInputDev->start(&mInputCallback);
        mInputHotRecoveries.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] uint64_t getInputHotRecoveryCount() const noexcept {
        return mInputHotRecoveries.load(std::memory_order_relaxed);
    }

private:
    struct InputPumpCallback final : public juce::AudioIODeviceCallback {
        explicit InputPumpCallback(DuplexDriftCompensatedAudioIODevice& o) noexcept : owner(o) {}

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override {
            owner.handleInputCallback(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, context);
        }

        void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
        void audioDeviceStopped() override {}

        DuplexDriftCompensatedAudioIODevice& owner;
    };

    struct OutputDriveCallback final : public juce::AudioIODeviceCallback {
        explicit OutputDriveCallback(DuplexDriftCompensatedAudioIODevice& o) noexcept : owner(o) {}

        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override {
            owner.handleOutputCallback(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, context);
        }

        void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
        void audioDeviceStopped() override {}

        DuplexDriftCompensatedAudioIODevice& owner;
    };

    void handleInputCallback(const float* const* inputChannelData,
                             int numInputChannels,
                             float* const* outputChannelData,
                             int numOutputChannels,
                             int numSamples,
                             const juce::AudioIODeviceCallbackContext& context) {
        mLastInputCallbackMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);

        if (mOutputOpened) {
            if (mNumActiveInputs > 0 && numSamples > 0) {
                mRingBuffer.push(inputChannelData, numInputChannels, numSamples);
            }
            return;
        }

        // Input-only configuration: forward directly to user callback
        const juce::ScopedTryLock sl(mCallbackLock);
        if (sl.isLocked() && mIsPlaying.load(std::memory_order_acquire) && mUserCallback != nullptr) {
            mUserCallback->audioDeviceIOCallbackWithContext(inputChannelData,
                                                            numInputChannels,
                                                            outputChannelData,
                                                            numOutputChannels,
                                                            numSamples,
                                                            context);
        }
    }

    void handleOutputCallback(const float* const* /*unusedInput*/,
                              int /*unusedNumIns*/,
                              float* const* outputChannelData,
                              int numOutputChannels,
                              int numSamples,
                              const juce::AudioIODeviceCallbackContext& context) {
        // Physically invariant guard against JUCE WASAPI AUDCLNT_E_DEVICE_INVALIDATED runaway spin storms:
        // No real hardware clock can consume >= 24 blocks AND >= 200 ms of audio frames in < 25% of
        // real-time wall-clock duration (> 4x faster than the speed of time!). Works identically whether
        // processBlock takes 1 us (Standby passthrough) or 1500 us (full 192 kHz 7.1.4 reverb).
        if (mStreamFaulted.load(std::memory_order_relaxed)) {
            clearOutputChannels(outputChannelData, numOutputChannels, numSamples);
            juce::Thread::sleep(2);
            return;
        }

        const int64_t nowTicks = juce::Time::getHighResolutionTicks();
        if (numSamples > 0) {
            if (mRateWindowStartTick == 0) {
                mRateWindowStartTick = nowTicks;
                mRateWindowFrames = 0;
                mRateWindowBlocks = 0;
            } else {
                mRateWindowFrames += numSamples;
                ++mRateWindowBlocks;
                const double windowAudioSec = static_cast<double>(mRateWindowFrames) / std::max(8000.0, mCurrentSampleRate);
                if (mRateWindowBlocks >= 24 && windowAudioSec >= 0.20) {
                    const double elapsedWallSec = juce::Time::highResolutionTicksToSeconds(nowTicks - mRateWindowStartTick);
                    if (elapsedWallSec >= 0.0 && elapsedWallSec < windowAudioSec * 0.25) {
                        mStreamFaulted.store(true, std::memory_order_release);
                        clearOutputChannels(outputChannelData, numOutputChannels, numSamples);
                        juce::Thread::sleep(2);
                        return;
                    }
                    mRateWindowStartTick = nowTicks;
                    mRateWindowFrames = 0;
                    mRateWindowBlocks = 0;
                }
            }
        }
        mLastOutputCallbackMs.store(juce::Time::getMillisecondCounterHiRes(), std::memory_order_relaxed);

        const float* const* inPtrs = nullptr;
        int numInsToPass = 0;

        if (mInputOpened && mNumActiveInputs > 0 && numSamples > 0) {
            if (numSamples <= mScratchInputBuffer.getNumSamples()) {
                mRingBuffer.pull(mScratchInputPtrs.data(), mNumActiveInputs, numSamples);
                inPtrs = mScratchInputPtrs.data();
                numInsToPass = mNumActiveInputs;
            }
        }

        const juce::ScopedTryLock sl(mCallbackLock);
        if (sl.isLocked() && mIsPlaying.load(std::memory_order_acquire) && mUserCallback != nullptr) {
            mUserCallback->audioDeviceIOCallbackWithContext(inPtrs,
                                                            numInsToPass,
                                                            outputChannelData,
                                                            numOutputChannels,
                                                            numSamples,
                                                            context);
        } else {
            clearOutputChannels(outputChannelData, numOutputChannels, numSamples);
        }
    }

    static void clearOutputChannels(float* const* outs, int numChans, int numSamples) noexcept {
        if (outs == nullptr || numSamples <= 0) {
            return;
        }
        for (int ch = 0; ch < numChans; ++ch) {
            if (outs[ch] != nullptr) {
                std::fill(outs[ch], outs[ch] + numSamples, 0.0f);
            }
        }
    }

    juce::AudioIODeviceType& mParentType;
    std::unique_ptr<juce::AudioIODevice> mOutputDev;
    std::unique_ptr<juce::AudioIODevice> mInputDev;
    juce::String mOutputDeviceName;
    juce::String mInputDeviceName;

    InputPumpCallback mInputCallback;
    OutputDriveCallback mOutputCallback;

    ElasticDriftRingBuffer mRingBuffer;
    juce::AudioBuffer<float> mScratchInputBuffer;
    std::vector<float*> mScratchInputPtrs;

    juce::CriticalSection mCallbackLock;
    juce::AudioIODeviceCallback* mUserCallback { nullptr };

    juce::BigInteger mRequestedInputChannels;
    juce::BigInteger mRequestedOutputChannels;
    int mRequestedBufferSize { 0 };
    double mCurrentSampleRate { 48000.0 };
    int mCurrentBufferSize { 512 };
    int mNumActiveInputs { 0 };
    bool mInputOpened { false };
    bool mOutputOpened { false };
    bool mIsOpen { false };
    std::atomic<bool> mIsPlaying { false };
    juce::String mLastError;

    // Stream health & storm detection
    int64_t mRateWindowStartTick { 0 };
    int64_t mRateWindowFrames { 0 };
    int mRateWindowBlocks { 0 };
    std::atomic<bool> mStreamFaulted { false };
    std::atomic<double> mLastOutputCallbackMs { 0.0 };
    std::atomic<double> mLastInputCallbackMs { 0.0 };
    std::atomic<uint64_t> mInputHotRecoveries { 0 };
};

class DriftCompensatedAudioIODeviceType final : public juce::AudioIODeviceType,
                                                private juce::AudioIODeviceType::Listener {
public:
    explicit DriftCompensatedAudioIODeviceType(std::unique_ptr<juce::AudioIODeviceType> innerType)
        : juce::AudioIODeviceType(innerType->getTypeName()),
          mInner(std::move(innerType)) {
        mInner->addListener(this);
    }

    ~DriftCompensatedAudioIODeviceType() override {
        if (mInner != nullptr) {
            mInner->removeListener(this);
        }
    }

    void scanForDevices() override {
        mInner->scanForDevices();
    }

    juce::StringArray getDeviceNames(bool wantInputNames) const override {
        return mInner->getDeviceNames(wantInputNames);
    }

    int getDefaultDeviceIndex(bool forInput) const override {
        return mInner->getDefaultDeviceIndex(forInput);
    }

    int getIndexOfDevice(juce::AudioIODevice* device, bool asInput) const override {
        if (const auto* d = dynamic_cast<const DuplexDriftCompensatedAudioIODevice*>(device)) {
            juce::AudioIODevice* sub = asInput ? d->getInnerInputDevice() : d->getInnerOutputDevice();
            return sub != nullptr ? mInner->getIndexOfDevice(sub, asInput) : -1;
        }
        return mInner->getIndexOfDevice(device, asInput);
    }

    bool hasSeparateInputsAndOutputs() const override {
        return mInner->hasSeparateInputsAndOutputs();
    }

    juce::AudioIODevice* createDevice(const juce::String& outputDeviceName,
                                      const juce::String& inputDeviceName) override {
        if (!mInner->hasSeparateInputsAndOutputs()) {
            return mInner->createDevice(outputDeviceName, inputDeviceName);
        }

        std::unique_ptr<juce::AudioIODevice> outDev;
        std::unique_ptr<juce::AudioIODevice> inDev;

        if (outputDeviceName.isNotEmpty()) {
            outDev.reset(mInner->createDevice(outputDeviceName, {}));
            if (outDev == nullptr) {
                return nullptr;
            }
        }

        if (inputDeviceName.isNotEmpty()) {
            inDev.reset(mInner->createDevice({}, inputDeviceName));
            if (inDev == nullptr) {
                return nullptr;
            }
        }

        if (outDev == nullptr && inDev == nullptr) {
            return nullptr;
        }

        const juce::String primaryName = outputDeviceName.isNotEmpty() ? outputDeviceName : inputDeviceName;
        return new DuplexDriftCompensatedAudioIODevice(primaryName,
                                                       getTypeName(),
                                                       *mInner,
                                                       std::move(outDev),
                                                       std::move(inDev),
                                                       outputDeviceName,
                                                       inputDeviceName);
    }

private:
    void audioDeviceListChanged() override {
        callDeviceChangeListeners();
    }

    std::unique_ptr<juce::AudioIODeviceType> mInner;
};

inline void installDriftCompensatedDeviceTypes(juce::AudioDeviceManager& deviceManager) {
#if JUCE_WINDOWS
    deviceManager.closeAudioDevice();

    // Ensure default JUCE device types exist, then wrap separate-clock Windows types in-place
    const auto& existing = deviceManager.getAvailableDeviceTypes();
    juce::Array<juce::AudioIODeviceType*> snapshot;
    for (auto* t : existing) {
        snapshot.add(t);
    }
    for (auto* t : snapshot) {
        deviceManager.removeAudioDeviceType(t);
    }

    auto addWrappedOrRaw = [&deviceManager](juce::AudioIODeviceType* rawType, bool wrap) {
        if (rawType == nullptr) {
            return;
        }
        std::unique_ptr<juce::AudioIODeviceType> owned(rawType);
        if (wrap && owned->hasSeparateInputsAndOutputs()) {
            deviceManager.addAudioDeviceType(std::make_unique<DriftCompensatedAudioIODeviceType>(std::move(owned)));
        } else {
            deviceManager.addAudioDeviceType(std::move(owned));
        }
    };

    addWrappedOrRaw(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::shared), true);
    addWrappedOrRaw(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::exclusive), true);
    addWrappedOrRaw(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(juce::WASAPIDeviceMode::sharedLowLatency), true);
    addWrappedOrRaw(juce::AudioIODeviceType::createAudioIODeviceType_DirectSound(), true);
    addWrappedOrRaw(juce::AudioIODeviceType::createAudioIODeviceType_ASIO(), false);

    for (auto* t : deviceManager.getAvailableDeviceTypes()) {
        t->scanForDevices();
    }
#else
    juce::ignoreUnused(deviceManager);
#endif
}

// ============================================================================
// 5. Standalone Sleep/Wake & Stream-Fault Recovery Controller
// ============================================================================
// Preserves the last valid AudioDeviceSetup (e.g. VB-Cable + Realtek/USB DAC)
// across OS suspend/resume cycles and transient D3->D0 endpoint wake failures
// (where AudioDeviceManager::setAudioDeviceSetup would otherwise wipe
// currentSetup via deleteCurrentDevice() on first failure), while monitoring
// duplex stream health and wall-clock sleep jumps.
class StandaloneAudioRecoveryController final : private juce::ChangeListener,
                                                private juce::Timer {
public:
    explicit StandaloneAudioRecoveryController(juce::AudioDeviceManager& dm,
                                               std::function<void()> onStreamRecovered = {},
                                               int watchdogIntervalMs = 250)
        : mDeviceManager(dm),
          mOnStreamRecovered(std::move(onStreamRecovered)),
          mLastWallClockMs(currentWallClockMs()),
          mPowerNotifier([this] { handleSystemSuspend(); },
                         [this] { handleSystemResume(); }) {
        captureCurrentSetupIfActive();
        mDeviceManager.addChangeListener(this);
        if (watchdogIntervalMs > 0) {
            startTimer(watchdogIntervalMs);
        }
    }

    ~StandaloneAudioRecoveryController() override {
        stopTimer();
        mDeviceManager.removeChangeListener(this);
    }

    StandaloneAudioRecoveryController(const StandaloneAudioRecoveryController&) = delete;
    StandaloneAudioRecoveryController& operator=(const StandaloneAudioRecoveryController&) = delete;

    // Public hooks for deterministic testing as well as OS power callbacks
    void handleSystemSuspend() {
        captureCurrentSetupIfActive();
        mIsSuspended = true;
        mPendingRecovery = false;
        mRetryIndex = 0;

        if (mDeviceManager.getCurrentAudioDevice() != nullptr) {
            const juce::ScopedValueSetter<bool> internalScope(mInternalOperation, true);
            mDeviceManager.closeAudioDevice();
        }
    }

    void handleSystemResume(double initialSettleDelayMs = 650.0) {
        mIsSuspended = false;
        mLastWallClockMs = currentWallClockMs();
        if (!mHasSavedSetup) {
            return;
        }
        mPendingRecovery = true;
        mRetryIndex = 0;
        mNextRecoveryTimeMs = juce::Time::getMillisecondCounterHiRes() + std::max(0.0, initialSettleDelayMs);
    }

    // Executes one watchdog evaluation tick (called automatically by juce::Timer and directly in unit tests)
    void tickWatchdog(double nowHiResMs = juce::Time::getMillisecondCounterHiRes(),
                      int64_t nowWallMs = currentWallClockMs()) {
        // 1. Detect unannounced OS sleep / hibernation via wall-clock jump (> 4.0 seconds)
        const int64_t wallDeltaMs = nowWallMs - mLastWallClockMs;
        mLastWallClockMs = nowWallMs;
        if (!mIsSuspended && wallDeltaMs > 4000) {
            // If the audio device faulted or closed across the unannounced sleep jump, schedule recovery
            auto* curDev = mDeviceManager.getCurrentAudioDevice();
            const auto* duplex = dynamic_cast<const DuplexDriftCompensatedAudioIODevice*>(curDev);
            if (curDev == nullptr || !curDev->isOpen() || !curDev->isPlaying()
                || (duplex != nullptr && duplex->hasFatalStreamFault())) {
                handleSystemResume(500.0);
            }
        }

        if (mIsSuspended) {
            return;
        }

        // 2. Service scheduled post-sleep / post-fault recovery with preserved AudioDeviceSetup
        if (mPendingRecovery) {
            if (nowHiResMs >= mNextRecoveryTimeMs) {
                attemptDeviceRecovery(nowHiResMs);
            }
            return;
        }

        // 3. Steady-state stream health monitoring
        auto* curDev = mDeviceManager.getCurrentAudioDevice();
        if (curDev != nullptr) {
            if (auto* duplex = dynamic_cast<DuplexDriftCompensatedAudioIODevice*>(curDev)) {
                if (duplex->hasFatalStreamFault()) {
                    scheduleFaultRecovery(nowHiResMs);
                    return;
                }
                // Non-disruptively hot-recover capture if input-only WaitForSingleObject timed out
                (void)duplex->checkAndRecoverInputStreamOnMessageThread();
                return;
            }

            if (!curDev->isOpen() || !curDev->isPlaying()) {
                scheduleFaultRecovery(nowHiResMs);
            }
        }
    }

    void refreshSavedSetupFromManager() {
        captureCurrentSetupIfActive();
    }

    [[nodiscard]] bool isSuspended() const noexcept { return mIsSuspended; }
    [[nodiscard]] bool isRecoveryPending() const noexcept { return mPendingRecovery; }
    [[nodiscard]] uint64_t getFullRecoveryCount() const noexcept { return mFullRecoveryCount; }
    [[nodiscard]] const juce::AudioDeviceManager::AudioDeviceSetup& getSavedSetup() const noexcept { return mSavedSetup; }

    [[nodiscard]] static int64_t currentWallClockMs() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

private:
    void timerCallback() override {
        tickWatchdog();
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        if (mInternalOperation || mIsSuspended || mPendingRecovery) {
            return;
        }

        if (mDeviceManager.getCurrentAudioDevice() != nullptr) {
            captureCurrentSetupIfActive();
        } else {
            const auto setup = mDeviceManager.getAudioDeviceSetup();
            if (setup.inputDeviceName.isEmpty() && setup.outputDeviceName.isEmpty()) {
                // User explicitly deselected all devices in Audio/MIDI Settings
                mHasSavedSetup = false;
            }
        }
    }

    void captureCurrentSetupIfActive() {
        if (auto* dev = mDeviceManager.getCurrentAudioDevice()) {
            if (dev->isOpen()) {
                mSavedSetup = mDeviceManager.getAudioDeviceSetup();
                mSavedDeviceType = mDeviceManager.getCurrentAudioDeviceType();
                mHasSavedSetup = (!mSavedSetup.inputDeviceName.isEmpty()
                               || !mSavedSetup.outputDeviceName.isEmpty());
            }
        }
    }

    void scheduleFaultRecovery(double nowHiResMs) {
        if (!mHasSavedSetup) {
            return;
        }
        mPendingRecovery = true;
        mRetryIndex = 0;
        mNextRecoveryTimeMs = nowHiResMs + 250.0;
    }

    void attemptDeviceRecovery(double nowHiResMs) {
        if (!mHasSavedSetup) {
            mPendingRecovery = false;
            return;
        }

        const juce::ScopedValueSetter<bool> internalScope(mInternalOperation, true);

        // Close any broken / invalidated handle cleanly before re-scanning woken endpoints
        mDeviceManager.closeAudioDevice();

        for (auto* type : mDeviceManager.getAvailableDeviceTypes()) {
            type->scanForDevices();
        }

        if (mSavedDeviceType.isNotEmpty() && mDeviceManager.getCurrentAudioDeviceType() != mSavedDeviceType) {
            mDeviceManager.setCurrentAudioDeviceType(mSavedDeviceType, false);
            mDeviceManager.closeAudioDevice();
        }

        const auto savedCopy = mSavedSetup;
        const juce::String err = mDeviceManager.setAudioDeviceSetup(savedCopy, false);

        if (err.isEmpty() && mDeviceManager.getCurrentAudioDevice() != nullptr
            && mDeviceManager.getCurrentAudioDevice()->isOpen()) {
            mPendingRecovery = false;
            mRetryIndex = 0;
            ++mFullRecoveryCount;
            captureCurrentSetupIfActive();
            if (mOnStreamRecovered) {
                mOnStreamRecovered();
            }
            return;
        }

        // Endpoint still transitioning D3 -> D0 (e.g. Realtek codec / USB DAC / HDMI audio after wake):
        // schedule progressive backoff retries while preserving mSavedSetup intact!
        static constexpr std::array<double, 8> kRetryDelaysMs = {
            600.0, 1000.0, 1500.0, 2500.0, 3500.0, 5000.0, 7000.0, 10000.0
        };

        if (mRetryIndex < static_cast<int>(kRetryDelaysMs.size())) {
            mNextRecoveryTimeMs = nowHiResMs + kRetryDelaysMs[static_cast<size_t>(mRetryIndex)];
            ++mRetryIndex;
        } else {
            // Exhausted retries (e.g. USB interface was physically unplugged while asleep):
            // fall back gracefully to system default audio device if available.
            mPendingRecovery = false;
            mRetryIndex = 0;
            const int numIns  = mSavedSetup.useDefaultInputChannels  ? 2 : mSavedSetup.inputChannels.countNumberOfSetBits();
            const int numOuts = mSavedSetup.useDefaultOutputChannels ? 2 : mSavedSetup.outputChannels.countNumberOfSetBits();
            (void)mDeviceManager.initialiseWithDefaultDevices(numIns, numOuts);
            captureCurrentSetupIfActive();
            if (mDeviceManager.getCurrentAudioDevice() != nullptr && mOnStreamRecovered) {
                ++mFullRecoveryCount;
                mOnStreamRecovered();
            }
        }
    }

    juce::AudioDeviceManager& mDeviceManager;
    std::function<void()> mOnStreamRecovered;

    juce::AudioDeviceManager::AudioDeviceSetup mSavedSetup;
    juce::String mSavedDeviceType;
    bool mHasSavedSetup { false };
    bool mInternalOperation { false };
    bool mIsSuspended { false };
    bool mPendingRecovery { false };
    int mRetryIndex { 0 };
    double mNextRecoveryTimeMs { 0.0 };
    int64_t mLastWallClockMs { 0 };
    uint64_t mFullRecoveryCount { 0 };

    WindowsPowerSuspendNotifier mPowerNotifier;
};

} // namespace rb26::standalone
