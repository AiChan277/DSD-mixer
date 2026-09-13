#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Output/OutputManager.h"
#include "UI/OutputBayStrip.h"
#include <vector>
#include <memory>

namespace dsd
{
    class OutputBayPanel : public juce::Component
    {
    public:
        OutputBayPanel(OutputManager& outManager, juce::AudioDeviceManager& deviceManager);
        ~OutputBayPanel() override = default;
        void updateMeters();
        void updateAllUI();
        void resized() override;
        void paint(juce::Graphics& g) override;
    private:
        OutputManager& outputManagerRef;
        juce::AudioDeviceManager& devMgrRef;
        std::vector<std::unique_ptr<OutputBayStrip>> outputStrips;
    };
}
