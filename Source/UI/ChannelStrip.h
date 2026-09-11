#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel/AudioChannel.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class ChannelStrip : public juce::Component
    {
    public:
        ChannelStrip(AudioChannel& channel);
        ~ChannelStrip() override = default;

        void updateMeterFromAudio();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioChannel& channelRef;

        // Top OLED Mini Section (from sketch)
        juce::Label chNumberBadge;
        juce::Label dbfsReadout;
        MeterComponent topMeter; // Mini dBFS & peak indicator

        // Functional Buttons (from sketch)
        juce::TextButton directMonitorBtn{"DM"};
        juce::TextButton muteBtn{"MUTE"};
        juce::TextButton disableOutputBtn{"OUT"};

        // Console utility switches
        juce::TextButton phaseInvertBtn{"Ø"};
        juce::TextButton monoBtn{"MONO"};

        // VST Plugin Rack button
        juce::TextButton vstRackBtn{"[ VST RACK ]"};

        // Fader & Bottom Label (from sketch)
        FaderComponent fader;
        juce::Label channelNameLabel;

        void setupButtons();
        void openVstRackWindow();
    };
} // namespace dsd
