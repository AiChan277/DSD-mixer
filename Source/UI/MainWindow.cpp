#include "UI/MainWindow.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    MainWindow::MainWindow(const juce::String& name, AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : DocumentWindow(name,
                         DSDLookAndFeel::getConsoleDarkBg(),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainView(devManager, audioEngine), true);

        setResizable(true, true);
        setResizeLimits(1024, 600, 3840, 2160);
        centreWithSize(1360, 720);
        setVisible(true);
    }

    void MainWindow::closeButtonPressed()
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
} // namespace dsd
