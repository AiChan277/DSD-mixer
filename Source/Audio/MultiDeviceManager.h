#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioTypes.h"
#include "Audio/AudioInputSource.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace dsd
{
    // Real-time lock-free ring buffer for bridging asynchronous device threads
    class AudioRingBuffer
    {
    public:
        AudioRingBuffer(int channels = 2, int capacity = 8192)
            : fifo(capacity), buffer(channels, capacity)
        {
            buffer.clear();
        }

        void reset()
        {
            fifo.reset();
            buffer.clear();
        }

        void write(const float* const* src, int numChannels, int numSamples)
        {
            if (numSamples <= 0 || src == nullptr) return;
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
        }

        int read(float* const* dst, int numChannels, int numSamples)
        {
            if (numSamples <= 0 || dst == nullptr) return 0;
            int start1, size1, start2, size2;
            fifo.prepareToRead(numSamples, start1, size1, start2, size2);
            const int readTotal = size1 + size2;

            const int copyCh = std::min(numChannels, buffer.getNumChannels());
            if (size1 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (dst[ch] != nullptr)
                        juce::FloatVectorOperations::copy(dst[ch], buffer.getReadPointer(ch, start1), size1);
                }
            }
            if (size2 > 0)
            {
                for (int ch = 0; ch < copyCh; ++ch)
                {
                    if (dst[ch] != nullptr)
                        juce::FloatVectorOperations::copy(dst[ch] + size1, buffer.getReadPointer(ch, start2), size2);
                }
            }
            fifo.finishedRead(readTotal);

            // If underflow, zero out remaining samples
            if (readTotal < numSamples)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (dst[ch] != nullptr)
                        juce::FloatVectorOperations::clear(dst[ch] + readTotal, numSamples - readTotal);
                }
            }
            return readTotal;
        }

        int getNumReady() const noexcept { return fifo.getNumReady(); }

    private:
        juce::AbstractFifo fifo;
        juce::AudioBuffer<float> buffer;
    };

    // Dedicated input source capturing from a specific Windows audio device
    class WindowsDeviceInputSource : public AudioInputSource, public juce::AudioIODeviceCallback
    {
    public:
        WindowsDeviceInputSource(const juce::String& deviceName, juce::AudioIODeviceType* deviceType);
        ~WindowsDeviceInputSource() override;

        void prepare(double sampleRate, int maxBlockSize) override;
        void releaseResources() override;
        void readBlock(juce::AudioBuffer<float>& buffer,
                       const juce::AudioBuffer<float>& deviceInput,
                       int numSamples) override;

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        bool isDeviceActive() const noexcept { return device != nullptr && device->isPlaying(); }
        juce::String getDeviceName() const noexcept { return devName; }

    private:
        juce::String devName;
        juce::AudioIODeviceType* devType{nullptr};
        std::unique_ptr<juce::AudioIODevice> device;
        AudioRingBuffer ringBuffer{2, 16384};

        bool openDevice();
        void closeDevice();
    };

    // Dedicated output sink rendering to a specific Windows audio device
    class WindowsDeviceOutputSink : public juce::AudioIODeviceCallback
    {
    public:
        WindowsDeviceOutputSink(const juce::String& deviceName, juce::AudioIODeviceType* deviceType);
        ~WindowsDeviceOutputSink() override;

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();
        void writeBlock(const juce::AudioBuffer<float>& buffer, int numSamples);

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* outputChannelData,
                                              int numOutputChannels,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext& context) override;

        bool isDeviceActive() const noexcept { return device != nullptr && device->isPlaying(); }
        juce::String getDeviceName() const noexcept { return devName; }

    private:
        juce::String devName;
        juce::AudioIODeviceType* devType{nullptr};
        std::unique_ptr<juce::AudioIODevice> device;
        AudioRingBuffer ringBuffer{2, 16384};

        bool openDevice();
        void closeDevice();
    };

    // Central manager for enumerating, routing, and sharing Windows audio devices
    class MultiDeviceManager
    {
    public:
        static MultiDeviceManager& getInstance();

        void initialize(juce::AudioDeviceManager& hostManager);
        void shutdown();

        juce::StringArray getAvailableInputDevices();
        juce::StringArray getAvailableOutputDevices();

        juce::AudioIODeviceType* getWasapiDeviceType();

        // Factory to create an input source for a channel
        std::unique_ptr<AudioInputSource> createInputSourceFor(const juce::String& selectionName);

        // Factory to create or get an output sink for an output bus
        std::unique_ptr<WindowsDeviceOutputSink> createOutputSinkFor(const juce::String& deviceName);

    private:
        MultiDeviceManager() = default;
        ~MultiDeviceManager() = default;

        juce::AudioDeviceManager* juceManagerRef{nullptr};
        std::mutex mutex;
    };
} // namespace dsd
