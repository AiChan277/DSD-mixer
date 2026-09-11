#include "Output/OutputManager.h"

namespace dsd
{
    OutputManager::OutputManager()
    {
        initializeDefaultOutputs(NUM_OUTPUT_BUSES_LEVEL1);
    }

    void OutputManager::initializeDefaultOutputs(int count)
    {
        outputs.clear();
        outputs.reserve(count);

        const struct { const char* name; int chOffset; } defaultOuts[] = {
            { "Studio Monitor", -1 },
            { "Headphones",     -1 },
            { "OBS / Stream",   -1 },
            { "Recording",      -1 }
        };

        for (int i = 0; i < count; ++i)
        {
            std::string name = (i < 4) ? defaultOuts[i].name : ("OUT " + std::to_string(i + 1));
            int offset = (i < 4) ? defaultOuts[i].chOffset : -1;
            outputs.push_back(std::make_unique<OutputBus>(i + 1, name, offset));
        }
    }

    void OutputManager::prepare(double sampleRate, int maxBlockSize)
    {
        for (auto& out : outputs)
        {
            if (out != nullptr)
                out->prepare(sampleRate, maxBlockSize);
        }
    }

    void OutputManager::releaseResources()
    {
        for (auto& out : outputs)
        {
            if (out != nullptr)
                out->releaseResources();
        }
    }

    void OutputManager::processOutputs(int numSamples)
    {
        for (auto& out : outputs)
        {
            if (out != nullptr)
                out->processBlock(numSamples);
        }
    }

    void OutputManager::writeToDeviceOutputs(float* const* outputChannelData, int numOutputChannels, int numSamples)
    {
        if (outputChannelData == nullptr || numOutputChannels <= 0)
            return;

        // Clear output hardware buffers first
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                std::fill_n(outputChannelData[ch], numSamples, 0.0f);
        }

        // Sum active output buses to physical device channels based on channel offset
        for (const auto& out : outputs)
        {
            if (out == nullptr || out->getMute())
                continue;

            const int offset = out->getDeviceChannelOffset();
            if (offset < 0)  // null/unassigned output
                continue;

            const auto& buf = out->getBuffer();

            // Left
            if (offset < numOutputChannels && outputChannelData[offset] != nullptr)
            {
                const float* srcL = buf.getReadPointer(0);
                for (int i = 0; i < numSamples; ++i)
                    outputChannelData[offset][i] += srcL[i];
            }

            // Right
            if (offset + 1 < numOutputChannels && outputChannelData[offset + 1] != nullptr)
            {
                const float* srcR = buf.getReadPointer(1);
                for (int i = 0; i < numSamples; ++i)
                    outputChannelData[offset + 1][i] += srcR[i];
            }
        }

        // Apply transparent output limiting protection so signals never digital-clip or wrap at DAC
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
            {
                float* p = outputChannelData[ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = p[i];
                    if (x > 0.988f)
                        p[i] = 0.988f + 0.0119f * std::tanh((x - 0.988f) / 0.0119f);
                    else if (x < -0.988f)
                        p[i] = -0.988f + 0.0119f * std::tanh((x + 0.988f) / 0.0119f);
                }
            }
        }
    }

    OutputBus* OutputManager::getOutput(int index) noexcept
    {
        if (index >= 0 && index < static_cast<int>(outputs.size()))
            return outputs[index].get();
        return nullptr;
    }

    const OutputBus* OutputManager::getOutput(int index) const noexcept
    {
        if (index >= 0 && index < static_cast<int>(outputs.size()))
            return outputs[index].get();
        return nullptr;
    }
} // namespace dsd
