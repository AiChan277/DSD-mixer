#include "UI/MainWindow.h"
#include "UI/DSDLookAndFeel.h"
#include "Session/AppSettings.h"
#include <BinaryData.h>

namespace dsd
{
    MainWindow::MainWindow(const juce::String& name, AudioDeviceManager& devManager, AudioEngine& audioEngine, control::ControlServer* controlServer)
        : DocumentWindow(name,
                         DSDLookAndFeel::getConsoleDarkBg(),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainView(devManager, audioEngine, controlServer), true);

        auto icon = juce::ImageCache::getFromMemory(BinaryData::LOGO_png, BinaryData::LOGO_pngSize);
        if (icon.isValid())
            setIcon(icon);

        setResizable(true, true);
        setResizeLimits(1024, 600, 3840, 2160);
        centreWithSize(1360, 720);
        setVisible(true);
    }

    void MainWindow::closeButtonPressed()
    {
        if (AppSettings::getInstance().getMinimizeToTray())
        {
            setVisible(false);
        }
        else
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    }
} // namespace dsd
