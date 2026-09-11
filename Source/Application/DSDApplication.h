#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioDeviceManager.h"
#include "Audio/AudioEngine.h"
#include "UI/MainWindow.h"
#include <memory>

namespace dsd
{
    class DSDApplication : public juce::JUCEApplication
    {
    public:
        DSDApplication() = default;
        ~DSDApplication() override = default;

        const juce::String getApplicationName() override       { return "DSD Mixer"; }
        const juce::String getApplicationVersion() override    { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override             { return false; }

        void initialise(const juce::String& commandLine) override;
        void shutdown() override;

        void systemRequestedQuit() override;
        void anotherInstanceStarted(const juce::String& commandLine) override;

    private:
        std::unique_ptr<AudioEngine> audioEngine;
        std::unique_ptr<AudioDeviceManager> deviceManager;
        std::unique_ptr<MainWindow> mainWindow;
    };
} // namespace dsd
