#include "UI/OutputBayStrip.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    OutputBayStrip::OutputBayStrip(OutputBus& bus)
        : outputBusRef(bus),
          stereoMeter(true) // Dual stereo meter
    {
        // 1. Bus Name Label (Clickable & Editable directly by user!)
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);
        busNameLabel.setJustificationType(juce::Justification::centred);
        busNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        busNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        busNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
        busNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        busNameLabel.setEditable(true); // User can click and edit the output name!
        busNameLabel.onTextChange = [this]()
        {
            outputBusRef.setName(busNameLabel.getText().toStdString());
        };
        addAndMakeVisible(busNameLabel);

        // 2. Setup Functional Buttons
        setupButtons();

        // 3. Stereo Meter
        stereoMeter.setClipResetCallback([this]()
        {
            outputBusRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(stereoMeter);

        // 4. Stereo Fader
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            outputBusRef.setFaderDb(db);
        });
        addAndMakeVisible(fader);
    }

    void OutputBayStrip::setupButtons()
    {
        // Mute Button (Red illuminated)
        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff450a0a));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setTooltip("Mute Output");
        muteBtn.onClick = [this]()
        {
            outputBusRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Monitor Button (Green illuminated)
        monitorBtn.setClickingTogglesState(true);
        monitorBtn.setToggleState(outputBusRef.getMonitor(), juce::dontSendNotification);
        monitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14532d));
        monitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        monitorBtn.setTooltip("Toggle Monitor Send");
        monitorBtn.onClick = [this]()
        {
            outputBusRef.setMonitor(monitorBtn.getToggleState());
        };
        addAndMakeVisible(monitorBtn);

        // Target Hardware Device Channel Offset Button
        int currentOffset = outputBusRef.getDeviceChannelOffset();
        deviceChBtn.setButtonText(juce::String::formatted("CH %d/%d", currentOffset + 1, currentOffset + 2));
        deviceChBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        deviceChBtn.setTooltip("Change physical output device channels");
        deviceChBtn.onClick = [this]() { openChannelConfigMenu(); };
        addAndMakeVisible(deviceChBtn);
    }

    void OutputBayStrip::openChannelConfigMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Physical Channels 1 & 2 (Default Main)");
        menu.addItem(2, "Physical Channels 3 & 4 (Headphones / Secondary)");
        menu.addItem(3, "Physical Channels 5 & 6");
        menu.addItem(4, "Physical Channels 7 & 8");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&deviceChBtn),
            [this](int result)
            {
                if (result >= 1 && result <= 4)
                {
                    int offset = (result - 1) * 2;
                    outputBusRef.setDeviceChannelOffset(offset);
                    deviceChBtn.setButtonText(juce::String::formatted("CH %d/%d", offset + 1, offset + 2));
                }
            });
    }

    void OutputBayStrip::updateMeterFromAudio()
    {
        const auto& mv = outputBusRef.getMeterValues();
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float hL = mv.peakHoldL.load(std::memory_order_relaxed);
        const float hR = mv.peakHoldR.load(std::memory_order_relaxed);
        const bool clip = mv.clipped.load(std::memory_order_relaxed);

        stereoMeter.setMeterValues(pL, pR, hL, hR, clip);
    }

    void OutputBayStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Device Out Name display (editable)
        busNameLabel.setBounds(area.removeFromTop(24));

        area.removeFromTop(4);

        // 2. Hardware Channel Assignment selector
        deviceChBtn.setBounds(area.removeFromTop(20));

        area.removeFromTop(6);

        // 3. Buttons: Mute & Monitor side-by-side
        auto btnRow = area.removeFromTop(26);
        const int btnW = (btnRow.getWidth() - 4) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(4);
        monitorBtn.setBounds(btnRow);

        area.removeFromTop(8);

        // 4. Dual Level Meters
        stereoMeter.setBounds(area.removeFromTop(76));

        area.removeFromTop(8);

        // 5. Stereo Fader
        fader.setBounds(area);
    }

    void OutputBayStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Authentic console output bay finish (slightly darker metal with precision border)
        g.setColour(juce::Colour(0xff27292f));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
