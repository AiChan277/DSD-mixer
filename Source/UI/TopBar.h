#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"

namespace dsd::control
{
    class ControlServer;
}

namespace dsd
{
    class TopBar : public juce::Component
    {
    public:
        TopBar(AudioDeviceManager& devManager, AudioEngine& audioEngine, control::ControlServer* controlServer = nullptr);
        ~TopBar() override = default;

        std::function<void()> onSessionLoaded;

        void updateStats();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioDeviceManager& deviceManagerRef;
        AudioEngine& audioEngineRef;
        control::ControlServer* controlServerRef{nullptr};

        juce::Label titleLabel;

        juce::TextButton settingsBtn{"AUDIO DEVICE"};
        juce::TextButton matrixBtn{"ROUTING MATRIX"};
        juce::TextButton perfBtn{"PERF MONITOR"};
        juce::TextButton stageInspectorBtn{"STAGE METERS"};
        juce::TextButton remoteBtn{"REMOTE"};
        juce::TextButton toolsBtn{"TOOLS"};
        juce::TextButton saveSessionBtn{"SAVE .DSD"};
        juce::TextButton loadSessionBtn{"LOAD .DSD"};

        juce::Label sampleRateLabel;
        juce::Label bufferSizeLabel;
        juce::Label engineStatusBadge;
        juce::Label cpuLoadLabel;

        juce::Rectangle<float> statusPodBounds;
        float sessionDividerX{0.0f};

        void openAudioSettingsDialog();
        void openRoutingMatrixDialog();
        void openPerformanceDialog();
        void openStageInspectorDialog();
        void openControlServerDialog();
        void showToolsMenu();
        void openToolDialog(std::unique_ptr<juce::Component> content, const juce::String& title);
        void onSaveSessionClicked();
        void onLoadSessionClicked();
    };
} // namespace dsd
