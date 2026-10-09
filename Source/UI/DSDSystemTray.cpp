#include "UI/DSDSystemTray.h"
#include "UI/MainWindow.h"
#include "Control/ControlServer.h"
#include "Session/AppSettings.h"
#include <BinaryData.h>

namespace dsd
{
    DSDSystemTray::DSDSystemTray(MainWindow& window, control::ControlServer* server)
        : mainWindowRef(window), controlServerRef(server)
    {
        auto icon = juce::ImageCache::getFromMemory(BinaryData::LOGO_png, BinaryData::LOGO_pngSize);
        if (icon.isValid())
            setIconImage(icon, icon);

        setIconTooltip("DSD Mixer - Digital Audio Mixing Engine");
    }

    void DSDSystemTray::mouseDoubleClick(const juce::MouseEvent& /*e*/)
    {
        mainWindowRef.setVisible(true);
        mainWindowRef.toFront(true);
    }

    void DSDSystemTray::mouseDown(const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
        {
            showContextMenu();
        }
    }

    void DSDSystemTray::showContextMenu()
    {
        juce::PopupMenu menu;

        const bool isWindowVisible = mainWindowRef.isVisible();
        menu.addItem(1, isWindowVisible ? "Hide DSD Mixer" : "Open DSD Mixer");

        menu.addSeparator();

        // Remote Server Status & Controls
        if (controlServerRef != nullptr)
        {
            const bool isRunning = controlServerRef->isRunning();
            const int port = controlServerRef->getPort();
            const int authClients = controlServerRef->getAuthenticatedClientCount();

            juce::String statusStr = "Remote Server: ";
            if (isRunning)
                statusStr += "Running (Port " + juce::String(port) + ", " + juce::String(authClients) + " client" + (authClients == 1 ? "" : "s") + ")";
            else
                statusStr += "Stopped";

            menu.addItem(100, statusStr, false, false);

            if (isRunning)
                menu.addItem(2, "Stop Remote Server");
            else
                menu.addItem(3, "Start Remote Server");
        }

        menu.addSeparator();

        // Close behavior preference
        const bool minimizeToTray = AppSettings::getInstance().getMinimizeToTray();
        menu.addItem(4, "Minimize to Tray on Close", true, minimizeToTray);

        menu.addSeparator();
        menu.addItem(99, "Exit DSD Mixer");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this](int result)
            {
                if (result == 1)
                {
                    bool vis = mainWindowRef.isVisible();
                    mainWindowRef.setVisible(!vis);
                    if (!vis)
                        mainWindowRef.toFront(true);
                }
                else if (result == 2)
                {
                    if (controlServerRef != nullptr)
                        controlServerRef->stop();
                }
                else if (result == 3)
                {
                    if (controlServerRef != nullptr)
                    {
                        int p = AppSettings::getInstance().getRemoteServerPort();
                        controlServerRef->start(p, "0.0.0.0");
                    }
                }
                else if (result == 4)
                {
                    bool cur = AppSettings::getInstance().getMinimizeToTray();
                    AppSettings::getInstance().setMinimizeToTray(!cur);
                }
                else if (result == 99)
                {
                    juce::JUCEApplication::getInstance()->systemRequestedQuit();
                }
            });
    }
} // namespace dsd
