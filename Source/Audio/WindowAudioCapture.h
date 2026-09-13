#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "Audio/AudioInputSource.h"
#include "Audio/MultiDeviceManager.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <thread>

struct IAudioClient;

namespace dsd
{
    enum class CaptureLatencyMode
    {
        UltraLow = 0, // ~3 - 5 ms buffer cushion (Minimum latency for gaming / live monitoring)
        Low = 1,      // ~7 - 8 ms buffer cushion (Balanced low latency)
        Standard = 2  // ~15 ms buffer cushion (Safe jitter tolerance)
    };

    struct RunningAppInfo
    {
        juce::uint32 processId{0};
        juce::String appName;
        juce::String windowTitle;
    };

    struct CaptureDiagnostics
    {
        juce::uint32 processId{0};
        juce::String appName;
        bool isCapturing{false};
        double captureSampleRate{48000.0};
        int captureChannels{2};
        int bitsPerSample{32};
        bool isFloat{true};

        // Latency Profile
        CaptureLatencyMode latencyMode{CaptureLatencyMode::UltraLow};
        juce::String latencyModeName{"Ultra-Low (3-5ms)"};

        // WASAPI buffer metrics
        uint32_t wasapiBufferFrames{0};
        double wasapiBufferMs{0.0};
        double wasapiPeriodMs{0.0};

        // Ring buffer metrics
        int ringBufferCapacity{0};
        int ringBufferOccupancy{0};
        double ringBufferOccupancyMs{0.0};
        int targetOccupancy{0};
        double targetOccupancyMs{0.0};

        // Clock drift & synchronization
        float smoothedOccupancy{0.0f};
        float clockDriftPpm{0.0f};
        uint64_t totalCaptureFrames{0};
        uint64_t totalEngineFrames{0};

        // Quality counters
        uint32_t underrunCount{0};
        uint32_t overrunCount{0};
        uint32_t discontinuityCount{0};
        uint32_t driftCorrections{0};
        uint32_t burstFlushedFrames{0};

        // Real QPC hardware packet age (ground truth time from render to GetBuffer)
        double hardwarePacketAgeMs{10.0};

        // Estimated total latency
        double estimatedCaptureLatencyMs{0.0};
    };

    class LowLatencyCaptureFifo
    {
    public:
        LowLatencyCaptureFifo(int channels = 2, int capacity = 4096)
            : fifo(capacity), buffer(channels, capacity)
        {
            buffer.clear();
        }

        void reset()
        {
            fifo.reset();
            buffer.clear();
            isBuffering.store(true, std::memory_order_relaxed);
            smoothedOccupancy.store(0.0f, std::memory_order_relaxed);
            totalFramesWritten.store(0, std::memory_order_relaxed);
            totalFramesRead.store(0, std::memory_order_relaxed);
            clockDriftPpm.store(0.0f, std::memory_order_relaxed);
            samplesSinceCorrection = 0;
        }

        void setTargetOccupancy(int samples) noexcept
        {
            targetOccupancy.store(std::clamp(samples, 64, buffer.getNumSamples() / 2), std::memory_order_relaxed);
        }

        int getTargetOccupancy() const noexcept { return targetOccupancy.load(std::memory_order_relaxed); }
        int getCapacity() const noexcept { return buffer.getNumSamples(); }
        int getNumReady() const noexcept { return fifo.getNumReady(); }

        void write(const float* const* src, int numChannels, int numSamples)
        {
            if (numSamples <= 0 || src == nullptr) return;

            const int freeSpace = fifo.getFreeSpace();
            if (numSamples > freeSpace)
            {
                overrunCount.fetch_add(1, std::memory_order_relaxed);
                int drop = numSamples - freeSpace;
                int d1, s1, d2, s2;
                fifo.prepareToRead(drop, d1, s1, d2, s2);
                fifo.finishedRead(s1 + s2);
            }

            int start1, size1, start2, size2;
            fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

            const int copyCh = std::min(numChannels, buffer.getNumChannels());
            if (size1 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (src[ch] != nullptr)
                        buffer.copyFrom(ch, start1, src[ch], size1);
                    else
                        buffer.clear(ch, start1, size1);
                }
                for (int ch = copyCh; ch < buffer.getNumChannels(); ++ch)
                    buffer.clear(ch, start1, size1);
            }
            if (size2 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (src[ch] != nullptr)
                        buffer.copyFrom(ch, start2, src[ch] + size1, size2);
                    else
                        buffer.clear(ch, start2, size2);
                }
                for (int ch = copyCh; ch < buffer.getNumChannels(); ++ch)
                    buffer.clear(ch, start2, size2);
            }
            fifo.finishedWrite(size1 + size2);
            totalFramesWritten.fetch_add(size1 + size2, std::memory_order_relaxed);

            // Startup burst flush & pre-roll exit:
            const int ready = fifo.getNumReady();
            const int target = targetOccupancy.load(std::memory_order_relaxed);

            if (isBuffering.load(std::memory_order_relaxed) && ready >= std::min(128, numSamples))
            {
                // If an initial burst of packets arrived, flush excess down to optimal peak cushion (target + 1 packet)
                const int normalPacket = (numSamples >= 480) ? 480 : std::max(128, numSamples);
                const int maxCushion = target + normalPacket;
                if (ready > maxCushion + 32)
                {
                    int excess = ready - maxCushion;
                    int d1, s1, d2, s2;
                    fifo.prepareToRead(excess, d1, s1, d2, s2);
                    fifo.finishedRead(s1 + s2);
                    burstFlushedFrames.fetch_add(s1 + s2, std::memory_order_relaxed);
                }
                smoothedOccupancy.store(static_cast<float>(target + normalPacket / 2), std::memory_order_relaxed);
                isBuffering.store(false, std::memory_order_release);
            }
        }

        int read(float* const* dst, int numChannels, int numSamples, double sampleRate)
        {
            if (numSamples <= 0 || dst == nullptr) return 0;

            if (isBuffering.load(std::memory_order_acquire))
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (dst[ch] != nullptr)
                        juce::FloatVectorOperations::clear(dst[ch], numSamples);
                }
                return 0;
            }

            int ready = fifo.getNumReady();
            const int target = targetOccupancy.load(std::memory_order_relaxed);

            // Dynamic backlog flush: Windows shared loopback delivers in ~480-sample (10ms) packets.
            // Natural sawtooth oscillates between target (trough) and target + 480 (peak).
            // If ready exceeds peak + safety margin, drain stale backlog so latency NEVER accumulates!
            constexpr int expectedPacket = 480;
            const int backlogFlushThreshold = target + expectedPacket + std::max(64, target / 2);
            if (ready > backlogFlushThreshold)
            {
                int excess = ready - (target + expectedPacket);
                int d1, s1, d2, s2;
                fifo.prepareToRead(excess, d1, s1, d2, s2);
                fifo.finishedRead(s1 + s2);
                burstFlushedFrames.fetch_add(s1 + s2, std::memory_order_relaxed);
                ready = fifo.getNumReady();
            }

            // Track smoothed occupancy (exponential moving average)
            float occ = static_cast<float>(ready);
            float prevSmoothed = smoothedOccupancy.load(std::memory_order_relaxed);
            constexpr float alpha = 0.003f;
            float newSmoothed = (prevSmoothed <= 0.1f) ? occ : ((1.0f - alpha) * prevSmoothed + alpha * occ);
            smoothedOccupancy.store(newSmoothed, std::memory_order_relaxed);

            // Real-time clock drift calculation in PPM:
            const uint64_t totalRead = totalFramesRead.load(std::memory_order_relaxed);
            const uint64_t totalWritten = totalFramesWritten.load(std::memory_order_relaxed);
            if (totalRead > static_cast<uint64_t>(sampleRate * 2.0))
            {
                int64_t delta = static_cast<int64_t>(totalWritten) - static_cast<int64_t>(totalRead + ready);
                double elapsedSec = static_cast<double>(totalRead) / sampleRate;
                float ppm = static_cast<float>((static_cast<double>(delta) / (elapsedSec * sampleRate)) * 1e6);
                clockDriftPpm.store(ppm, std::memory_order_relaxed);
            }

            // Micro-steering drift adjustment around center of sawtooth (target + expectedPacket / 2)
            samplesSinceCorrection += numSamples;
            int dropSampleCount = 0;
            int duplicateSampleCount = 0;

            const float expectedCenter = static_cast<float>(target + expectedPacket / 2);
            constexpr float deadband = 32.0f; // ~0.67 ms
            if (samplesSinceCorrection > 2000 && ready > numSamples + 32)
            {
                if (newSmoothed > expectedCenter + deadband)
                {
                    dropSampleCount = 1;
                    samplesSinceCorrection = 0;
                    driftCorrectionCount.fetch_add(1, std::memory_order_relaxed);
                }
                else if (newSmoothed < expectedCenter - deadband && ready > numSamples)
                {
                    duplicateSampleCount = 1;
                    samplesSinceCorrection = 0;
                    driftCorrectionCount.fetch_add(1, std::memory_order_relaxed);
                }
            }

            int readFromFifo = numSamples + dropSampleCount - duplicateSampleCount;
            int start1, size1, start2, size2;
            fifo.prepareToRead(readFromFifo, start1, size1, start2, size2);
            const int readTotal = size1 + size2;

            const int copyCh = std::min(numChannels, buffer.getNumChannels());
            if (size1 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (dst[ch] != nullptr)
                        juce::FloatVectorOperations::copy(dst[ch], buffer.getReadPointer(ch, start1), std::min(size1, numSamples));
                }
            }
            if (size2 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    const int offsetInDst = size1;
                    const int toCopy = std::min(size2, std::max(0, numSamples - offsetInDst));
                    if (dst[ch] != nullptr && toCopy > 0)
                        juce::FloatVectorOperations::copy(dst[ch] + offsetInDst, buffer.getReadPointer(ch, start2), toCopy);
                }
            }
            fifo.finishedRead(readTotal);
            totalFramesRead.fetch_add(numSamples, std::memory_order_relaxed);

            // Handle micro-steering crossfade at end of buffer
            if (dropSampleCount > 0 && numSamples >= 16)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (dst[ch] != nullptr)
                    {
                        for (int i = 0; i < 16; ++i)
                        {
                            float w = static_cast<float>(i) / 16.0f;
                            dst[ch][numSamples - 16 + i] = (1.0f - w) * dst[ch][numSamples - 16 + i];
                        }
                    }
                }
            }
            else if (duplicateSampleCount > 0 && numSamples >= 16)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (dst[ch] != nullptr)
                    {
                        dst[ch][numSamples - 1] = dst[ch][numSamples - 2];
                    }
                }
            }

            // Underrun handling:
            if (readTotal < readFromFifo)
            {
                underrunCount.fetch_add(1, std::memory_order_relaxed);
                int validSamples = std::min(readTotal, numSamples);
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (dst[ch] != nullptr)
                    {
                        if (validSamples >= 16)
                        {
                            for (int i = 0; i < 16; ++i)
                                dst[ch][validSamples - 16 + i] *= (1.0f - static_cast<float>(i) / 16.0f);
                        }
                        juce::FloatVectorOperations::clear(dst[ch] + validSamples, numSamples - validSamples);
                    }
                }
            }

            return readTotal;
        }

        float getSmoothedOccupancy() const noexcept { return smoothedOccupancy.load(std::memory_order_relaxed); }
        float getClockDriftPpm() const noexcept { return clockDriftPpm.load(std::memory_order_relaxed); }
        uint64_t getTotalFramesWritten() const noexcept { return totalFramesWritten.load(std::memory_order_relaxed); }
        uint64_t getTotalFramesRead() const noexcept { return totalFramesRead.load(std::memory_order_relaxed); }
        uint32_t getUnderrunCount() const noexcept { return underrunCount.load(std::memory_order_relaxed); }
        uint32_t getOverrunCount() const noexcept { return overrunCount.load(std::memory_order_relaxed); }
        uint32_t getBurstFlushedFrames() const noexcept { return burstFlushedFrames.load(std::memory_order_relaxed); }
        uint32_t getDriftCorrections() const noexcept { return driftCorrectionCount.load(std::memory_order_relaxed); }

    private:
        juce::AbstractFifo fifo;
        juce::AudioBuffer<float> buffer;
        std::atomic<bool> isBuffering{true};
        std::atomic<int> targetOccupancy{192}; // ~4.0 ms trough cushion at 48 kHz (Low-Latency)
        std::atomic<float> smoothedOccupancy{0.0f};
        std::atomic<float> clockDriftPpm{0.0f};
        std::atomic<uint64_t> totalFramesWritten{0};
        std::atomic<uint64_t> totalFramesRead{0};

        std::atomic<uint32_t> underrunCount{0};
        std::atomic<uint32_t> overrunCount{0};
        std::atomic<uint32_t> burstFlushedFrames{0};
        std::atomic<uint32_t> driftCorrectionCount{0};

        int samplesSinceCorrection{0};
    };

    // Window Audio Capture (OBS-style Process Loopback)
    class WindowAudioCapture : public AudioInputSource
    {
    public:
        WindowAudioCapture(juce::uint32 targetPid, const juce::String& processName);
        ~WindowAudioCapture() override;

        void prepare(double sampleRate, int maxBlockSize) override;
        void releaseResources() override;
        void readBlock(juce::AudioBuffer<float>& targetBuffer,
                       const juce::AudioBuffer<float>& deviceInputBuffer,
                       int numSamples) override;

        bool isCapturing() const noexcept { return isRunning.load(std::memory_order_relaxed); }
        juce::uint32 getTargetPid() const noexcept { return pid; }
        juce::String getProcessName() const noexcept { return procName; }

        CaptureDiagnostics getDiagnostics() const;

        void setLatencyMode(CaptureLatencyMode mode);
        CaptureLatencyMode getLatencyMode() const noexcept { return latencyMode.load(std::memory_order_relaxed); }

        static void setGlobalLatencyMode(CaptureLatencyMode mode);
        static CaptureLatencyMode getGlobalLatencyMode() noexcept { return globalLatencyMode.load(std::memory_order_relaxed); }

        int calculateTargetOccupancy(CaptureLatencyMode mode, int blockSize, double sampleRate) const;

        // Enumerate running applications with top-level windows
        static std::vector<RunningAppInfo> getRunningApplications();

    private:
        juce::uint32 pid{0};
        juce::String procName;

        double targetSampleRate{48000.0};
        double captureSampleRate{48000.0};
        int captureChannels{2};
        int bitsPerSample{32};
        bool isNativeFloat{true};

        std::atomic<uint32_t> wasapiBufferFrames{0};
        std::atomic<double> wasapiPeriodMs{10.0};
        std::atomic<double> latestHardwarePacketAgeMs{10.0};
        std::atomic<uint32_t> discontinuityCount{0};

        std::atomic<CaptureLatencyMode> latencyMode{CaptureLatencyMode::Low};
        static inline std::atomic<CaptureLatencyMode> globalLatencyMode{CaptureLatencyMode::Low};
        int currentMaxBlockSize{256};

        LowLatencyCaptureFifo ringBuffer{2, 4096};
        juce::LagrangeInterpolator interpolator[2];
        juce::AudioBuffer<float> resampleBuffer;

        std::atomic<bool> isRunning{false};
        std::atomic<bool> shouldStop{false};
        std::thread captureThread;

        void threadLoop();
        bool activateProcessLoopback(IAudioClient** outClient);
    };
} // namespace dsd
