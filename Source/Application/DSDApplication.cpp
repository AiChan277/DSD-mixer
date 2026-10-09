#include "Application/DSDApplication.h"
#include "Audio/MultiDeviceManager.h"
#include "Plugin/PluginManager.h"
#include "Session/AppSettings.h"
#include "UI/MainView.h"
#include "UI/ChannelStrip.h"
#include "UI/ControlServerDialog.h"
#include <BinaryData.h>

namespace dsd
{
    void DSDApplication::initialise(const juce::String& commandLine)
    {
        // 1. Initialize application preferences
        auto& settings = AppSettings::getInstance();

        // 2. Create Audio Engine
        audioEngine = std::make_unique<AudioEngine>();

        // 3. Create and initialise Audio Device Manager (WASAPI default)
        deviceManager = std::make_unique<AudioDeviceManager>();
        deviceManager->initialize(2, 2);

        // 4. Initialize Multi-Device Manager for Windows Audio streams
        MultiDeviceManager::getInstance().initialize(deviceManager->getJuceManager());

        // 5. Auto-scan VST3 plugins in the background
        PluginManager::getInstance().startBackgroundAutoScan();

        // 6. Register audio engine as the real-time device callback
        deviceManager->addAudioCallback(audioEngine.get());

        // 7. Create and start DSD Control Server for remote control surfaces (Samsung A9)
        const int serverPort = settings.getRemoteServerPort();
        controlServer = std::make_unique<control::ControlServer>(*audioEngine, serverPort);

        if (settings.getStartRemoteServerAuto())
        {
            bool started = controlServer->start(serverPort, "0.0.0.0");
            if (!started)
            {
                DBG("[DSDApplication] Warning: Remote Control Server failed to bind to port " << serverPort);
            }
        }

        // 8. Create Main GUI Window
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *deviceManager, *audioEngine, controlServer.get());

        // 9. Initialize System Tray Integration
        systemTray = std::make_unique<DSDSystemTray>(*mainWindow, controlServer.get());

        juce::File debugLog("D:\\Project\\DSD-mixer\\debug_log.txt");
        debugLog.appendText("initialise called with commandLine: '" + commandLine + "'\n");

        if (commandLine.contains("--snapshot") || juce::File("D:\\Project\\DSD-mixer\\trigger_snapshot.txt").existsAsFile())
        {
            debugLog.appendText("Snapshot triggered!\n");
            if (mainWindow != nullptr)
            {
                if (auto* mainView = dynamic_cast<MainView*>(mainWindow->getContentComponent()))
                {
                    mainView->updateAllUI();
                    if (auto* firstStrip = mainView->getFirstChannelStrip())
                    {
                        auto stripImg = firstStrip->createComponentSnapshot(firstStrip->getLocalBounds());
                        juce::File stripFile("D:\\Project\\DSD-mixer\\channel_strip_snapshot.png");
                        stripFile.deleteFile();
                        juce::FileOutputStream stream(stripFile);
                        if (stream.openedOk())
                        {
                            juce::PNGImageFormat png;
                            png.writeImageToStream(stripImg, stream);
                            debugLog.appendText("Wrote channel_strip_snapshot.png successfully!\n");
                        }
                    }

                    auto fullImg = mainView->createComponentSnapshot(mainView->getLocalBounds());
                    juce::File fullFile("D:\\Project\\DSD-mixer\\dsd_mixer_snapshot.png");
                    fullFile.deleteFile();
                    juce::FileOutputStream stream(fullFile);
                    if (stream.openedOk())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream(fullImg, stream);
                        debugLog.appendText("Wrote dsd_mixer_snapshot.png successfully!\n");
                    }

                    if (controlServer != nullptr)
                    {
                        ControlServerDialog serverDlg(*controlServer);
                        auto srvImg = serverDlg.createComponentSnapshot(serverDlg.getLocalBounds());
                        juce::File srvFile("D:\\Project\\DSD-mixer\\control_server_snapshot.png");
                        srvFile.deleteFile();
                        juce::FileOutputStream srvStream(srvFile);
                        if (srvStream.openedOk())
                        {
                            juce::PNGImageFormat png;
                            png.writeImageToStream(srvImg, srvStream);
                            debugLog.appendText("Wrote control_server_snapshot.png successfully!\n");
                        }
                    }
                }
            }
            juce::File("D:\\Project\\DSD-mixer\\trigger_snapshot.txt").deleteFile();
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return;
        }
    }

    void DSDApplication::shutdown()
    {
        systemTray.reset();

        if (controlServer != nullptr)
        {
            controlServer->stop();
            controlServer.reset();
        }

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
