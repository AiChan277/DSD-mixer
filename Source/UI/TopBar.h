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
        juce::TextButton matrixBtn{"ROUTING MATRIX"};
        juce::TextButton perfBtn{"PERF MONITOR"};
        juce::TextButton stageInspectorBtn{"STAGE METERS"};
        juce::TextButton saveSessionBtn{"SAVE .DSD"};
        juce::TextButton loadSessionBtn{"LOAD .DSD"};

        juce::Label sampleRateLabel;
        juce::Label bufferSizeLabel;
        juce::Label engineStatusBadge;
        juce::Label cpuLoadLabel;

        void openAudioSettingsDialog();
        void openRoutingMatrixDialog();
        void openPerformanceDialog();
        void openStageInspectorDialog();
        void onSaveSessionClicked();
        void onLoadSessionClicked();
    };
} // namespace dsd
