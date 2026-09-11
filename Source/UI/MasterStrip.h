#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Master/MasterBus.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class MasterStrip : public juce::Component
    {
    public:
        MasterStrip(MasterBus& masterBus);
        ~MasterStrip() override = default;

        void updateMeterFromAudio();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        MasterBus& masterRef;

        // Top Device Out Name (from sketch)
        juce::Label deviceOutNameLabel;

        // Functional Buttons (from sketch)
        juce::TextButton muteBtn{"MUTE"};
        juce::TextButton monitorBtn{"MONITOR"};

        // Dual L & R Level Meters (from sketch)
        MeterComponent stereoMeter; // Dual L/R vertical LED bars

        // Stereo Master Fader (from sketch)
        FaderComponent masterFader;

        // Bottom section label
        juce::Label masterBottomLabel;

        void setupButtons();
    };
} // namespace dsd
