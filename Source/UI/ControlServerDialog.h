#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Control/ControlServer.h"
#include <vector>

namespace dsd
{
    class ControlServerDialog : public juce::Component, public juce::Timer
    {
    public:
        explicit ControlServerDialog(control::ControlServer& server);
        ~ControlServerDialog() override;

        void timerCallback() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        control::ControlServer& serverRef;

        // Header & Server State
        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::Label serverStatusBadge;
        juce::TextButton toggleServerBtn{"STOP SERVER"};
        
        // Port configuration
        juce::Label portLabel;
        juce::TextEditor portEditor;
        juce::TextButton applyPortBtn{"SET PORT"};

        // Network Information
        juce::Label networkSectionLabel;
        juce::Label ipAddressesLabel;
        juce::Label instructionLabel;

        // PIN Code Pairing
        juce::Label pinSectionLabel;
        juce::Label pinDisplayLabel;
        juce::TextButton regenPinBtn{"REGENERATE PIN"};
        juce::TextEditor customPinEditor;
        juce::TextButton setPinBtn{"SET PIN"};

        // Connected Devices List
        juce::Label clientsSectionLabel;
        juce::Label noClientsLabel;

        struct ClientRow
        {
            std::unique_ptr<juce::Label> nameLabel;
            std::unique_ptr<juce::Label> idLabel;
            std::unique_ptr<juce::Label> statusBadge;
            std::unique_ptr<juce::Label> subLabel;
        };

        std::vector<ClientRow> clientRows;
        juce::Component clientsContainer;
        juce::Viewport clientsViewport;

        void updateStatusUI();
        void updateClientsList();
        void generateNewRandomPin();
        void applyCustomPin();
        void toggleServerRunning();
        void applyPort();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlServerDialog)
    };
} // namespace dsd
