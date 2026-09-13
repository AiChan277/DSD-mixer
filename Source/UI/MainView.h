#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"
#include "UI/TopBar.h"
#include "UI/ChannelStrip.h"
#include "UI/OutputBayPanel.h"
#include "UI/DSDLookAndFeel.h"
#include <vector>
#include <memory>

namespace dsd
{
    class MainView : public juce::Component, public juce::Timer
    {
    public:
        MainView(AudioDeviceManager& devManager, AudioEngine& audioEngine);
        ~MainView() override;

        void timerCallback() override;
        void updateAllUI();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioDeviceManager& deviceManagerRef;
        AudioEngine& audioEngineRef;

        DSDLookAndFeel customLookAndFeel;

        TopBar topBar;
        std::vector<std::unique_ptr<ChannelStrip>> channelStrips;
        OutputBayPanel outputBayPanel;

        juce::Viewport channelsViewport;
        juce::Component channelsContainer;
    };
} // namespace dsd
