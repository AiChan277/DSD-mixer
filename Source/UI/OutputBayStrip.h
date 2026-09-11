#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Output/OutputBus.h"
#include "UI/MeterComponent.h"
#include "UI/FaderComponent.h"

namespace dsd
{
    class OutputBayStrip : public juce::Component
    {
    public:
        OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager);
        ~OutputBayStrip() override = default;
        void updateMeterFromAudio();
        void refreshDeviceList();
        void resized() override;
        void paint(juce::Graphics& g) override;
    private:
        OutputBus& outputBusRef;
        juce::AudioDeviceManager& devMgrRef;
        juce::ComboBox outputDeviceSelector;
        juce::Label busNameLabel;
        juce::TextButton muteBtn{"MUTE"};
        juce::TextButton monitorBtn{"MON"};
        MeterComponent stereoMeter;
        FaderComponent fader;
        void setupButtons();
        void onDeviceSelected();
    };
}
