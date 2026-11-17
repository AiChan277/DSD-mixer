#include "Application/DSDApplication.h"
#include "Application/CrashReporter.h"
#include "Audio/MultiDeviceManager.h"
#include "Plugin/PluginManager.h"
#include "Session/AppSettings.h"
#include "UI/MainView.h"
#include "UI/ChannelStrip.h"
#include "UI/ControlServerDialog.h"
#include "UI/PerformanceMonitorDialog.h"
#include "UI/RoutingMatrixDialog.h"
#include "UI/ToolsDialogs.h"
#include "Session/SceneManager.h"
#include <BinaryData.h>

namespace dsd
{
    void DSDApplication::initialise(const juce::String& commandLine)
    {
        // 0. Crash reports go to %APPDATA%\DSDMixer\crashes
        installCrashReporter();

        // 1. Initialize application preferences
        auto& settings = AppSettings::getInstance();

        // 2. Create Audio Engine
        audioEngine = std::make_unique<AudioEngine>();

        // 2b. Restore scene memory saved by the previous run
        SceneManager::getInstance().loadFromFile(SceneManager::getDefaultFile());

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

        // Smoke-test mode: `--quit-after=<seconds>` runs normally (message loop,
        // audio device, timers) and then quits through the normal shutdown path.
        if (commandLine.contains("--quit-after="))
        {
            const int seconds = commandLine.fromFirstOccurrenceOf("--quit-after=", false, false).getIntValue();
            juce::Timer::callAfterDelay(juce::jmax(1, seconds) * 1000, []()
            {
                if (auto* app = juce::JUCEApplication::getInstance())
                    app->systemRequestedQuit();
            });
        }

        // Developer snapshot mode: `--snapshot=<folder>` renders the main view and
        // the tool dialogs to PNG files in <folder> (default: working directory)
        // and quits. Normal launches write nothing.
        if (commandLine.contains("--snapshot"))
        {
            juce::File outDir = juce::File::getCurrentWorkingDirectory();
            const auto arg = commandLine.fromFirstOccurrenceOf("--snapshot=", false, false).upToFirstOccurrenceOf(" ", false, false).unquoted();
            if (arg.isNotEmpty())
                outDir = juce::File::getCurrentWorkingDirectory().getChildFile(arg);
            outDir.createDirectory();

            auto writePng = [&outDir](const juce::Image& img, const juce::String& name)
            {
                auto file = outDir.getChildFile(name);
                file.deleteFile();
                juce::FileOutputStream stream(file);
                if (stream.openedOk())
                {
                    juce::PNGImageFormat png;
                    png.writeImageToStream(img, stream);
                }
            };

            if (mainWindow != nullptr)
            {
                if (auto* mainView = dynamic_cast<MainView*>(mainWindow->getContentComponent()))
                {
                    mainView->updateAllUI();
                    if (auto* firstStrip = mainView->getFirstChannelStrip())
                        writePng(firstStrip->createComponentSnapshot(firstStrip->getLocalBounds()), "channel_strip_snapshot.png");
                    writePng(mainView->createComponentSnapshot(mainView->getLocalBounds()), "dsd_mixer_snapshot.png");
                }
            }

            if (controlServer != nullptr)
            {
                ControlServerDialog serverDlg(*controlServer);
                writePng(serverDlg.createComponentSnapshot(serverDlg.getLocalBounds()), "control_server_snapshot.png");
            }

            {
                PerformanceMonitorDialog dlg(*audioEngine);
                writePng(dlg.createComponentSnapshot(dlg.getLocalBounds()), "performance_monitor_snapshot.png");
            }
            {
                RoutingMatrixDialog dlg(audioEngine->getRoutingEngine(), audioEngine->getChannelManager(), audioEngine->getOutputManager());
                writePng(dlg.createComponentSnapshot(dlg.getLocalBounds()), "routing_matrix_snapshot.png");
            }
            {
                ScenesDialog dlg(*audioEngine);
                writePng(dlg.createComponentSnapshot(dlg.getLocalBounds()), "scenes_snapshot.png");
            }
            {
                AutoDuckDialog dlg(*audioEngine);
                writePng(dlg.createComponentSnapshot(dlg.getLocalBounds()), "auto_duck_snapshot.png");
            }
            {
                LoudnessDialog dlg(*audioEngine);
                writePng(dlg.createComponentSnapshot(dlg.getLocalBounds()), "loudness_snapshot.png");
            }

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

        // Release JUCE objects held by static singletons while JUCE is still alive.
        AppSettings::getInstance().shutdown();
    }

    void DSDApplication::systemRequestedQuit()
    {
        quit();
    }

    void DSDApplication::anotherInstanceStarted(const juce::String& /*commandLine*/)
    {
    }
} // namespace dsd
