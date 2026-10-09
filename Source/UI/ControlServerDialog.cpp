#include "UI/ControlServerDialog.h"
#include "UI/DSDLookAndFeel.h"
#include <juce_core/juce_core.h>

namespace dsd
{
    ControlServerDialog::ControlServerDialog(control::ControlServer& server)
        : serverRef(server)
    {
        // 1. Header
        titleLabel.setText("DSD CONTROL SURFACE - SERVER & PAIRING", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1A1D24));
        addAndMakeVisible(titleLabel);

        subtitleLabel.setText("DSDCP v1 WebSocket Remote Control for Samsung Galaxy A9 and Touch Devices", juce::dontSendNotification);
        subtitleLabel.setFont(juce::FontOptions(11.0f));
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff4A505C));
        addAndMakeVisible(subtitleLabel);

        // Server Status Badge
        serverStatusBadge.setJustificationType(juce::Justification::centred);
        serverStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(serverStatusBadge);

        // Toggle Server Button (uses OnButton.png / OffButton.png via DSDLookAndFeel)
        toggleServerBtn.onClick = [this]() { toggleServerRunning(); };
        addAndMakeVisible(toggleServerBtn);

        // Port Configuration
        portLabel.setText("PORT:", juce::dontSendNotification);
        portLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        portLabel.setColour(juce::Label::textColourId, juce::Colour(0xff2A2D33));
        addAndMakeVisible(portLabel);

        portEditor.setText(juce::String(serverRef.getPort()), juce::dontSendNotification);
        portEditor.setFont(juce::FontOptions(12.0f));
        portEditor.setInputRestrictions(5, "0123456789");
        portEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12141A));
        portEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        portEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2E323C));
        addAndMakeVisible(portEditor);

        applyPortBtn.onClick = [this]() { applyPort(); };
        addAndMakeVisible(applyPortBtn);

        // 2. Network Section
        networkSectionLabel.setText("NETWORK CONNECTION INFO", juce::dontSendNotification);
        networkSectionLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        networkSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentBlue());
        addAndMakeVisible(networkSectionLabel);

        ipAddressesLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
        ipAddressesLabel.setJustificationType(juce::Justification::topLeft);
        ipAddressesLabel.setBorderSize(juce::BorderSize<int>(6, 8, 6, 8));
        ipAddressesLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3FB950));
        ipAddressesLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0D1117));
        ipAddressesLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff21262D));
        addAndMakeVisible(ipAddressesLabel);

        instructionLabel.setText("Ensure the Samsung Galaxy A9 is connected to the same local Wi-Fi or USB network.\nEnter one of the above WebSocket URLs in the A9 control client.", juce::dontSendNotification);
        instructionLabel.setFont(juce::FontOptions(10.5f));
        instructionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff3E434D));
        addAndMakeVisible(instructionLabel);

        // 3. PIN Pairing Section
        pinSectionLabel.setText("SECURITY & PIN PAIRING", juce::dontSendNotification);
        pinSectionLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        pinSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9A3412));
        addAndMakeVisible(pinSectionLabel);

        pinDisplayLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold));
        pinDisplayLabel.setJustificationType(juce::Justification::centred);
        pinDisplayLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161B22));
        pinDisplayLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xffD29922));
        pinDisplayLabel.setColour(juce::Label::textColourId, juce::Colour(0xffE3B341));
        addAndMakeVisible(pinDisplayLabel);

        regenPinBtn.onClick = [this]() { generateNewRandomPin(); };
        addAndMakeVisible(regenPinBtn);

        customPinEditor.setFont(juce::FontOptions(12.0f));
        customPinEditor.setInputRestrictions(6, "0123456789");
        customPinEditor.setTextToShowWhenEmpty("Custom PIN", juce::Colour(0xff6E7681));
        customPinEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12141A));
        customPinEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        customPinEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2E323C));
        addAndMakeVisible(customPinEditor);

        setPinBtn.onClick = [this]() { applyCustomPin(); };
        addAndMakeVisible(setPinBtn);

        // 4. Connected Devices Section
        clientsSectionLabel.setText("CONNECTED REMOTE DEVICES", juce::dontSendNotification);
        clientsSectionLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        clientsSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentBlue());
        addAndMakeVisible(clientsSectionLabel);

        noClientsLabel.setText("No remote devices currently connected.\nOpen the A9 control interface and enter the pairing PIN above.", juce::dontSendNotification);
        noClientsLabel.setFont(juce::FontOptions(11.5f));
        noClientsLabel.setJustificationType(juce::Justification::centred);
        noClientsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6E7681));
        noClientsLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0D1117));
        noClientsLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff21262D));
        addAndMakeVisible(noClientsLabel);

        clientsViewport.setViewedComponent(&clientsContainer, false);
        clientsViewport.setScrollBarsShown(true, false);
        addAndMakeVisible(clientsViewport);

        updateStatusUI();
        updateClientsList();

        startTimerHz(2); // 2Hz refresh for UI
        setSize(580, 620);
    }

    ControlServerDialog::~ControlServerDialog()
    {
        stopTimer();
    }

    void ControlServerDialog::timerCallback()
    {
        updateStatusUI();
        updateClientsList();
    }

    void ControlServerDialog::updateStatusUI()
    {
        const bool running = serverRef.isRunning();
        const int port = serverRef.getPort();

        if (running)
        {
            serverStatusBadge.setText(juce::String::formatted("[ONLINE] SERVER RUNNING (PORT %d)", port), juce::dontSendNotification);
            serverStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff0F2D1A));
            serverStatusBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xff238636));
            serverStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xff3FB950));
            toggleServerBtn.setButtonText("STOP SERVER");
            toggleServerBtn.setToggleState(true, juce::dontSendNotification);
        }
        else
        {
            serverStatusBadge.setText("[OFFLINE] SERVER STOPPED", juce::dontSendNotification);
            serverStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff3B1414));
            serverStatusBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xffDA3633));
            serverStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffF85149));
            toggleServerBtn.setButtonText("START SERVER");
            toggleServerBtn.setToggleState(false, juce::dontSendNotification);
        }

        // Update valid LAN IPv4 addresses (excluding loopback, link-local, multicast 224-239, and broadcast 240-255)
        auto ips = juce::IPAddress::getAllAddresses(false);
        juce::String ipText = "Available LAN WebSocket Endpoints:\n";
        int validCount = 0;
        for (const auto& ip : ips)
        {
            if (ip.isIPv6)
                continue;

            const uint8_t b0 = ip.address[0];
            const uint8_t b1 = ip.address[1];
            const uint8_t b3 = ip.address[3];

            if (b0 == 0 || b0 == 127 || b0 >= 224 || b3 == 255)
                continue;
            if (b0 == 169 && b1 == 254)
                continue;

            ipText += "  * ws://" + ip.toString() + ":" + juce::String(port) + "\n";
            ++validCount;
        }
        if (validCount == 0)
            ipText += "  * ws://127.0.0.1:" + juce::String(port) + " (localhost only)\n";

        ipAddressesLabel.setText(ipText.trimEnd(), juce::dontSendNotification);

        juce::String pin = serverRef.getPin();
        if (pin.isEmpty())
            pinDisplayLabel.setText("NO PIN (OPEN)", juce::dontSendNotification);
        else
        {
            // Format with spaces e.g. "8 3 9 1 0 2"
            juce::String spacedPin;
            for (int i = 0; i < pin.length(); ++i)
            {
                spacedPin += pin.substring(i, i + 1);
                if (i < pin.length() - 1)
                    spacedPin += "  ";
            }
            pinDisplayLabel.setText(spacedPin, juce::dontSendNotification);
        }
    }

    void ControlServerDialog::updateClientsList()
    {
        auto clients = serverRef.getAllClients();
        const bool hasClients = !clients.empty();

        noClientsLabel.setVisible(!hasClients);
        clientsViewport.setVisible(hasClients);

        if (!hasClients)
        {
            clientRows.clear();
            clientsContainer.removeAllChildren();
            return;
        }

        // Rebuild or resize client rows
        const int rowHeight = 36;
        const int totalHeight = static_cast<int>(clients.size()) * rowHeight;
        clientsContainer.setSize(clientsViewport.getWidth() > 0 ? clientsViewport.getWidth() - 10 : 540, totalHeight);

        clientRows.resize(clients.size());

        for (size_t i = 0; i < clients.size(); ++i)
        {
            const auto& c = clients[i];
            auto& row = clientRows[i];

            if (!row.nameLabel)
            {
                row.nameLabel = std::make_unique<juce::Label>();
                row.nameLabel->setFont(juce::FontOptions(12.0f, juce::Font::bold));
                row.nameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
                clientsContainer.addAndMakeVisible(row.nameLabel.get());

                row.idLabel = std::make_unique<juce::Label>();
                row.idLabel->setFont(juce::FontOptions(10.0f));
                row.idLabel->setColour(juce::Label::textColourId, juce::Colour(0xff8B949E));
                clientsContainer.addAndMakeVisible(row.idLabel.get());

                row.statusBadge = std::make_unique<juce::Label>();
                row.statusBadge->setFont(juce::FontOptions(10.0f, juce::Font::bold));
                row.statusBadge->setJustificationType(juce::Justification::centred);
                clientsContainer.addAndMakeVisible(row.statusBadge.get());

                row.subLabel = std::make_unique<juce::Label>();
                row.subLabel->setFont(juce::FontOptions(10.0f));
                row.subLabel->setColour(juce::Label::textColourId, juce::Colour(0xffA0A6B2));
                clientsContainer.addAndMakeVisible(row.subLabel.get());
            }

            row.nameLabel->setText(juce::String(c->name) + " (" + juce::String(c->type) + ")", juce::dontSendNotification);
            row.idLabel->setText("ID: " + juce::String(c->clientId), juce::dontSendNotification);

            if (c->authenticated)
            {
                row.statusBadge->setText("AUTH OK", juce::dontSendNotification);
                row.statusBadge->setColour(juce::Label::backgroundColourId, juce::Colour(0xff0F2D1A));
                row.statusBadge->setColour(juce::Label::outlineColourId, juce::Colour(0xff238636));
                row.statusBadge->setColour(juce::Label::textColourId, juce::Colour(0xff3FB950));
            }
            else
            {
                row.statusBadge->setText("PENDING PIN", juce::dontSendNotification);
                row.statusBadge->setColour(juce::Label::backgroundColourId, juce::Colour(0xff3B2E14));
                row.statusBadge->setColour(juce::Label::outlineColourId, juce::Colour(0xffD29922));
                row.statusBadge->setColour(juce::Label::textColourId, juce::Colour(0xffE3B341));
            }

            juce::String subStr = "Events: " + juce::String(static_cast<int>(c->subscriptions.size()));
            if (c->meteringSubscribed)
                subStr += " | Meters: " + juce::String(c->meterIntervalMs) + "ms";
            else
                subStr += " | Meters: off";
            row.subLabel->setText(subStr, juce::dontSendNotification);

            const int y = static_cast<int>(i) * rowHeight;
            const int w = clientsContainer.getWidth();
            row.nameLabel->setBounds(8, y + 2, 220, 18);
            row.idLabel->setBounds(8, y + 18, 220, 16);
            row.statusBadge->setBounds(240, y + 6, 88, 22);
            row.subLabel->setBounds(340, y + 6, w - 348, 22);
        }
    }

    void ControlServerDialog::generateNewRandomPin()
    {
        juce::Random rng;
        int pinVal = rng.nextInt(juce::Range<int>(100000, 999999));
        serverRef.setPin(juce::String(pinVal));
        updateStatusUI();
    }

    void ControlServerDialog::applyCustomPin()
    {
        auto custom = customPinEditor.getText().trim();
        if (custom.length() == 6)
        {
            serverRef.setPin(custom);
            customPinEditor.clear();
            updateStatusUI();
        }
        else if (custom.isEmpty())
        {
            serverRef.setPin("");
            updateStatusUI();
        }
    }

    void ControlServerDialog::toggleServerRunning()
    {
        if (serverRef.isRunning())
        {
            serverRef.stop();
        }
        else
        {
            int p = portEditor.getText().getIntValue();
            if (p <= 0 || p > 65535)
                p = control::DSDCP_DEFAULT_PORT;
            serverRef.start(p, "0.0.0.0");
        }
        updateStatusUI();
        updateClientsList();
    }

    void ControlServerDialog::applyPort()
    {
        int p = portEditor.getText().getIntValue();
        if (p > 0 && p <= 65535)
        {
            if (serverRef.isRunning())
            {
                serverRef.stop();
                serverRef.start(p, "0.0.0.0");
            }
            updateStatusUI();
        }
    }

    void ControlServerDialog::resized()
    {
        const int pad = 16;
        int y = pad;
        const int w = getWidth() - (pad * 2);

        // Header
        titleLabel.setBounds(pad, y, w, 22);
        y += 22;
        subtitleLabel.setBounds(pad, y, w, 16);
        y += 24;

        // Server Status & Toggle (total width w = 548)
        serverStatusBadge.setBounds(pad, y, 232, 28);
        toggleServerBtn.setBounds(pad + 240, y, 114, 28);

        portLabel.setBounds(pad + 362, y + 4, 42, 20);
        portEditor.setBounds(pad + 406, y, 56, 28);
        applyPortBtn.setBounds(pad + 468, y, 80, 28);
        y += 38;

        // Network Info Pod
        networkSectionLabel.setBounds(pad, y, w, 18);
        y += 20;
        ipAddressesLabel.setBounds(pad, y, w, 68);
        y += 72;
        instructionLabel.setBounds(pad, y, w, 28);
        y += 36;

        // PIN Security Pod
        pinSectionLabel.setBounds(pad, y, w, 18);
        y += 20;

        pinDisplayLabel.setBounds(pad, y, 214, 36);
        regenPinBtn.setBounds(pad + 222, y + 4, 136, 28);
        customPinEditor.setBounds(pad + 366, y + 4, 94, 28);
        setPinBtn.setBounds(pad + 468, y + 4, 80, 28);
        y += 48;

        // Connected Devices Pod
        clientsSectionLabel.setBounds(pad, y, w, 18);
        y += 22;

        const int clientsBoxH = getHeight() - y - pad;
        noClientsLabel.setBounds(pad, y, w, clientsBoxH);
        clientsViewport.setBounds(pad, y, w, clientsBoxH);
    }

    void ControlServerDialog::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());

        // Inner panels / dividers
        g.setColour(juce::Colour(0xff2A2E38));
        g.drawRect(bounds, 1.0f);
    }

} // namespace dsd
