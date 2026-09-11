#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Output/OutputBus.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class OutputBayStrip : public juce::Component
    {
    public:
        OutputBayStrip(OutputBus& bus);
        ~OutputBayStrip() override = default;

        void updateMeterFromAudio();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        OutputBus& outputBusRef;

        juce::Label busNameLabel;
        juce::TextButton muteBtn{"MUTE"};
        juce::TextButton monitorBtn{"MONITOR"};
        juce::TextButton deviceChBtn{"CH 1/2"};

        MeterComponent stereoMeter; // Dual L/R vertical LED bars
        FaderComponent fader;

        void setupButtons();
        void openChannelConfigMenu();
    };
} // namespace dsd
