#pragma once

#include "Output/OutputBus.h"
#include <vector>
#include <memory>

namespace dsd
{
    class OutputManager
    {
    public:
        OutputManager();
        ~OutputManager() = default;

        void initializeDefaultOutputs(int count = NUM_OUTPUT_BUSES_LEVEL1);

        void prepare(double sampleRate, int maxBlockSize);
        void releaseResources();

        void processOutputs(int numSamples);

        // Copies audio from output buses to device hardware output channels
        void writeToDeviceOutputs(float* const* outputChannelData, int numOutputChannels, int numSamples);

        int getNumOutputs() const noexcept { return static_cast<int>(outputs.size()); }
        OutputBus* getOutput(int index) noexcept;
        const OutputBus* getOutput(int index) const noexcept;

    private:
        std::vector<std::unique_ptr<OutputBus>> outputs;
    };
} // namespace dsd
