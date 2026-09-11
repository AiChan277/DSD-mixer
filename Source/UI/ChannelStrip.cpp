#include "UI/ChannelStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "DSP/GainProcessor.h"

namespace dsd
{
    ChannelStrip::ChannelStrip(AudioChannel& channel)
        : channelRef(channel),
          topMeter(false) // mono meter for channel display
    {
        // 1. Channel Number Badge
        chNumberBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()), juce::dontSendNotification);
        chNumberBadge.setJustificationType(juce::Justification::centred);
        chNumberBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        chNumberBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentBlue().withAlpha(0.35f));
        chNumberBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentBlue());
        chNumberBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(chNumberBadge);

        // 2. dBFS Numerical readout
        dbfsReadout.setText("-inf", juce::dontSendNotification);
        dbfsReadout.setJustificationType(juce::Justification::centred);
        dbfsReadout.setFont(juce::FontOptions(10.0f));
        dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        addAndMakeVisible(dbfsReadout);

        // 3. Mini dBFS Meter + Peak LED (click to reset clip)
        topMeter.setClipResetCallback([this]()
        {
            channelRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(topMeter);

        // 4. Functional Buttons setup
        setupButtons();

        // 5. Fader setup
        fader.setValue(channelRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            channelRef.setFaderDb(db);
        });
        addAndMakeVisible(fader);

        // 6. Channel Name Bottom Label
        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);
        channelNameLabel.setJustificationType(juce::Justification::centred);
        channelNameLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        channelNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        channelNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getConsoleBevel());
        channelNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        channelNameLabel.setEditable(true); // User can click and edit channel name!
        channelNameLabel.onTextChange = [this]()
        {
            channelRef.setName(channelNameLabel.getText().toStdString());
        };
        addAndMakeVisible(channelNameLabel);
    }

    void ChannelStrip::setupButtons()
    {
        // Direct Monitor Button (Green illuminated)
        directMonitorBtn.setClickingTogglesState(true);
        directMonitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        directMonitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        directMonitorBtn.setTooltip("Direct Monitor (PFL/Zero-latency monitor)");
        directMonitorBtn.onClick = [this]()
        {
            channelRef.setDirectMonitor(directMonitorBtn.getToggleState());
        };
        addAndMakeVisible(directMonitorBtn);

        // Mute Button (Red illuminated)
        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff450a0a));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setTooltip("Mute Channel");
        muteBtn.onClick = [this]()
        {
            channelRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Disable Output / Route Button (Blue illuminated - active means output routed)
        disableOutputBtn.setClickingTogglesState(true);
        disableOutputBtn.setToggleState(true, juce::dontSendNotification); // default routed to Master
        disableOutputBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        disableOutputBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentBlue());
        disableOutputBtn.setTooltip("Route Output to Master");
        disableOutputBtn.onClick = [this]()
        {
            // If toggle is ON, output is ENABLED (disableOutput = false)
            channelRef.setDisableOutput(!disableOutputBtn.getToggleState());
        };
        addAndMakeVisible(disableOutputBtn);

        // VST Plugin Rack Button
        vstRackBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        vstRackBtn.setTooltip("Open Channel VST3 Plugin Rack");
        vstRackBtn.onClick = [this]()
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "VST Plugin Rack",
                channelNameLabel.getText() + " Plugin Chain\n(Milestone v0.2: VST3 loader, bypass, & editor)",
                "OK");
        };
        addAndMakeVisible(vstRackBtn);
    }

    void ChannelStrip::updateMeterFromAudio()
    {
        const auto& mv = channelRef.getMeterValues();
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float peakMax = std::max(pL, pR);
        const float hL = mv.peakHoldL.load(std::memory_order_relaxed);
        const float hR = mv.peakHoldR.load(std::memory_order_relaxed);
        const bool clip = mv.clipped.load(std::memory_order_relaxed);

        topMeter.setMeterValues(pL, pR, hL, hR, clip);

        // Update digital readout in dBFS
        if (peakMax <= 0.001f)
        {
            dbfsReadout.setText("-oo", juce::dontSendNotification);
            dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        }
        else
        {
            const float db = GainProcessor::linearToDb(peakMax);
            dbfsReadout.setText(juce::String(db, 1), juce::dontSendNotification);
            if (db >= -0.5f)
                dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterRed());
            else if (db >= -6.0f)
                dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
            else
                dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        }
    }

    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Top Section: OLED panel housing meter & badges
        auto oledArea = area.removeFromTop(80);
        auto meterCol = oledArea.removeFromLeft(36);
        topMeter.setBounds(meterCol.removeFromTop(56));
        dbfsReadout.setBounds(meterCol);

        oledArea.removeFromLeft(4);
        auto badgesCol = oledArea;
        directMonitorBtn.setBounds(badgesCol.removeFromTop(24).removeFromLeft(42));
        chNumberBadge.setBounds(badgesCol.removeFromTop(24).removeFromRight(50));

        area.removeFromTop(6);

        // 2. Buttons Row: MUTE and OUT side-by-side
        auto btnRow = area.removeFromTop(28);
        const int btnW = (btnRow.getWidth() - 4) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(4);
        disableOutputBtn.setBounds(btnRow);

        area.removeFromTop(6);

        // 3. VST Rack Button
        vstRackBtn.setBounds(area.removeFromTop(24));

        area.removeFromTop(8);

        // 4. Bottom Section: Channel Name Label
        channelNameLabel.setBounds(area.removeFromBottom(28));

        area.removeFromBottom(6);

        // 5. Remaining vertical space: Long-throw Fader
        fader.setBounds(area);
    }

    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Channel strip background (brushed broadcast panel)
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        // Top OLED inset background
        auto oledBounds = bounds.reduced(4.0f).removeFromTop(80.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(oledBounds, 4.0f);
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(oledBounds, 4.0f, 1.0f);

        // Bevel border around whole strip
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
