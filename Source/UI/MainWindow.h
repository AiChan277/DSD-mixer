#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"
#include "UI/MainView.h"

namespace dsd::control
{
    class ControlServer;
}

namespace dsd
{
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, AudioDeviceManager& devManager, AudioEngine& audioEngine, control::ControlServer* controlServer = nullptr);
        ~MainWindow() override = default;

        void closeButtonPressed() override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
} // namespace dsd
