#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Output/OutputBus.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"
#include "UI/DSDConsoleButton.h"
#include "UI/ChannelStrip.h"

namespace dsd
{
    class OutputBayStrip : public juce::Component
    {
    public:
        OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager);
        ~OutputBayStrip() override = default;

        void updateMeterFromAudio();
        void refreshDeviceList();
        void updateUIFromBus();
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        OutputBus& outputBusRef;
        juce::AudioDeviceManager& devMgrRef;

        DynamicComboBox outputDeviceSelector;

        // Recessed OLED display card
        juce::Rectangle<float> oledBounds;
        juce::Label busIdBadge;
        juce::Label faderReadout;
        juce::Label busNameLabel;
        juce::Label busStatusLabel;
        juce::Label outPeakDbfsLabel;

        // Stereo Level Meter (L / R)
        MeterComponent stereoMeter;

        // Broadcast Console Buttons: MUTE and MON
        DSDConsoleButton muteBtn   { "MUTE", ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };
        DSDConsoleButton monitorBtn{ "MON",  ConsoleButtonStyle::SquareAsset, ButtonLabelPlacement::Below };

        // Console Fader
        FaderComponent fader;

        // Bottom Gain readout pill
        juce::Rectangle<float> gainReadoutBounds;
        juce::Label gainReadout;

        void setupButtons();
        void onDeviceSelected();
        void updateGainReadout(float db);
        void updateBusStatusLabel();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputBayStrip)
    };
} // namespace dsd
