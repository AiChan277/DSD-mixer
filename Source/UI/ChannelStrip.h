#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Channel/AudioChannel.h"
#include "Audio/WindowAudioCapture.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class DynamicComboBox : public juce::ComboBox
    {
    public:
        std::function<void()> onBeforePopup;
        void showPopup() override
        {
            if (onBeforePopup)
                onBeforePopup();
            juce::ComboBox::showPopup();
        }
    };

    class ChannelStrip : public juce::Component
    {
    public:
        ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager);
        ~ChannelStrip() override;
        void updateMeterFromAudio();
        void refreshDeviceList();
        void updateUIFromChannel();
        void resized() override;
        void paint(juce::Graphics& g) override;
    private:
        AudioChannel& channelRef;
        juce::AudioDeviceManager& devMgrRef;
        DynamicComboBox inputDeviceSelector;
        // OLED Screen Card components
        juce::Label chNumberBadge;
        juce::Label gainReadout;
        juce::Label channelNameLabel;
        MeterComponent topMeter;
        juce::Label dbfsReadout;

        // Sub-function utility buttons (compact row)
        juce::TextButton vstRackBtn{"VST"};
        juce::TextButton directMonitorBtn{"DM"};
        juce::TextButton phaseInvertBtn{"PH"};
        juce::TextButton monoBtn{"MONO"};

        // Hardware Broadcast Fader
        FaderComponent fader;

        // Large Broadcast Bottom Buttons (DHD RX2/SX2 Console)
        juce::TextButton onBtn{"ON"};
        juce::TextButton muteBtn{"OFF"};

        std::vector<RunningAppInfo> runningApps;

        void setupButtons();
        void onDeviceSelected();
        void openVstRackWindow();

        juce::Component::SafePointer<juce::DocumentWindow> activeRackWindow;
    };
}
