#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"
#include "UI/TopBar.h"
#include "UI/ChannelStrip.h"
#include "UI/OutputBayPanel.h"
#include "UI/DSDLookAndFeel.h"
#include "Control/ControlServer.h"
#include "Control/StateObserver.h"
#include <vector>
#include <memory>

namespace dsd
{
    class MainView : public juce::Component, public juce::Timer, public control::StateObserver::Listener
    {
    public:
        MainView(AudioDeviceManager& devManager, AudioEngine& audioEngine, control::ControlServer* controlServer = nullptr);
        ~MainView() override;

        void timerCallback() override;
        void updateAllUI();

        // StateObserver::Listener implementation
        void onStateEvents(const std::vector<juce::String>& events) override;

        void resized() override;
        void paint(juce::Graphics& g) override;

        ChannelStrip* getFirstChannelStrip() { return channelStrips.empty() ? nullptr : channelStrips[0].get(); }

    private:
        AudioDeviceManager& deviceManagerRef;
        AudioEngine& audioEngineRef;
        control::ControlServer* controlServerRef { nullptr };

        DSDLookAndFeel customLookAndFeel;

        TopBar topBar;
        std::vector<std::unique_ptr<ChannelStrip>> channelStrips;
        OutputBayPanel outputBayPanel;

        juce::Viewport channelsViewport;
        juce::Component channelsContainer;
    };
} // namespace dsd
