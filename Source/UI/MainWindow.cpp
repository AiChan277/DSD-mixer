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
        setResizeLimits(760, 580, 2560, 1600);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void MainWindow::closeButtonPressed()
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
} // namespace dsd
