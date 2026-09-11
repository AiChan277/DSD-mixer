#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Channel/AudioChannel.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class ChannelStrip : public juce::Component
    {
    public:
        ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager);
        ~ChannelStrip() override = default;
        void updateMeterFromAudio();
        void refreshDeviceList();
        void resized() override;
        void paint(juce::Graphics& g) override;
    private:
        AudioChannel& channelRef;
        juce::AudioDeviceManager& devMgrRef;
        juce::ComboBox inputDeviceSelector;
        juce::Label chNumberBadge;
        juce::Label dbfsReadout;
        MeterComponent topMeter;
        juce::TextButton directMonitorBtn{"DM"};
        juce::TextButton muteBtn{"MUTE"};
        juce::TextButton disableOutputBtn{"OUT"};
        juce::TextButton phaseInvertBtn;
        juce::TextButton monoBtn{"MONO"};
        juce::TextButton vstRackBtn{"VST RACK"};
        juce::Label gainReadout;
        FaderComponent fader;
        juce::Label channelNameLabel;
        void setupButtons();
        void onDeviceSelected();
        void openVstRackWindow();
    };
}
