#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"

namespace dsd
{
    class TopBar : public juce::Component
    {
    public:
        TopBar(AudioDeviceManager& devManager, AudioEngine& audioEngine);
        ~TopBar() override = default;

        void updateStats();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioDeviceManager& deviceManagerRef;
        AudioEngine& audioEngineRef;

        juce::Label titleLabel;
        juce::TextButton settingsBtn{"AUDIO DEVICE"};
        juce::Label sampleRateLabel;
        juce::Label bufferSizeLabel;
        juce::Label engineStatusBadge;
        juce::Label cpuLoadLabel;

        float currentCpuPercent{0.0f};

        void openAudioSettingsDialog();
    };
} // namespace dsd
