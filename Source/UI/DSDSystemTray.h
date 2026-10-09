#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

namespace dsd
{
    namespace control
    {
        class ControlServer;
    }

    class MainWindow;

    class DSDSystemTray : public juce::SystemTrayIconComponent
    {
    public:
        DSDSystemTray(MainWindow& window, control::ControlServer* server);
        ~DSDSystemTray() override = default;

        void mouseDoubleClick(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        void showContextMenu();

        MainWindow& mainWindowRef;
        control::ControlServer* controlServerRef{nullptr};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSDSystemTray)
    };
} // namespace dsd
