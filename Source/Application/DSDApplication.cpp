#include "Application/DSDApplication.h"
#include "Audio/MultiDeviceManager.h"

namespace dsd
{
    void DSDApplication::initialise(const juce::String& /*commandLine*/)
    {
        // 1. Create Audio Engine
        audioEngine = std::make_unique<AudioEngine>();

        // 2. Create and initialise Audio Device Manager (WASAPI default)
        deviceManager = std::make_unique<AudioDeviceManager>();
        deviceManager->initialize(2, 2);

        // 3. Initialize Multi-Device Manager for Windows Audio streams
        MultiDeviceManager::getInstance().initialize(deviceManager->getJuceManager());

        // 4. Register audio engine as the real-time device callback
        deviceManager->addAudioCallback(audioEngine.get());

        // 5. Create Main GUI Window
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *deviceManager, *audioEngine);
    }

    void DSDApplication::shutdown()
    {
        MultiDeviceManager::getInstance().shutdown();

        if (deviceManager != nullptr && audioEngine != nullptr)
        {
            deviceManager->removeAudioCallback(audioEngine.get());
        }

        mainWindow.reset();
        deviceManager.reset();
        audioEngine.reset();
    }

    void DSDApplication::systemRequestedQuit()
    {
        quit();
    }

    void DSDApplication::anotherInstanceStarted(const juce::String& /*commandLine*/)
    {
    }
} // namespace dsd
