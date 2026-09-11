#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "Audio/AudioInputSource.h"
#include "Audio/MultiDeviceManager.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <thread>

namespace dsd
{
    struct RunningAppInfo
    {
        DWORD processId{0};
        juce::String appName;
        juce::String windowTitle;
    };

    // Window Audio Capture (OBS-style Process Loopback)
    class WindowAudioCapture : public AudioInputSource
    {
    public:
        WindowAudioCapture(DWORD targetPid, const juce::String& processName);
        ~WindowAudioCapture() override;

        void prepare(double sampleRate, int maxBlockSize) override;
        void releaseResources() override;
        void readBlock(juce::AudioBuffer<float>& targetBuffer,
                       const juce::AudioBuffer<float>& deviceInputBuffer,
                       int numSamples) override;

        bool isCapturing() const noexcept { return isRunning.load(std::memory_order_relaxed); }
        DWORD getTargetPid() const noexcept { return pid; }
        juce::String getProcessName() const noexcept { return procName; }

        // Enumerate running applications with top-level windows
        static std::vector<RunningAppInfo> getRunningApplications();

    private:
        DWORD pid{0};
        juce::String procName;

        double targetSampleRate{48000.0};
        double captureSampleRate{48000.0};
        int captureChannels{2};

        AudioRingBuffer ringBuffer{2, 32768};
        juce::LagrangeInterpolator interpolator[2];
        juce::AudioBuffer<float> resampleBuffer;

        std::atomic<bool> isRunning{false};
        std::atomic<bool> shouldStop{false};
        std::thread captureThread;

        void threadLoop();
        bool activateProcessLoopback(IAudioClient** outClient);
    };
} // namespace dsd
