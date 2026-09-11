#include "UI/ChannelStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/PluginRackDialog.h"
#include "DSP/GainProcessor.h"

namespace dsd
{
    ChannelStrip::ChannelStrip(AudioChannel& channel)
        : channelRef(channel),
          topMeter(false)
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
        dbfsReadout.setText("-oo", juce::dontSendNotification);
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
        channelNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        channelNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        channelNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getConsoleBevel());
        channelNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        channelNameLabel.setEditable(true);
        channelNameLabel.onTextChange = [this]()
        {
            channelRef.setName(channelNameLabel.getText().toStdString());
        };
        addAndMakeVisible(channelNameLabel);
    }

    void ChannelStrip::setupButtons()
    {
        // Direct Monitor Button (Green)
        directMonitorBtn.setClickingTogglesState(true);
        directMonitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        directMonitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        directMonitorBtn.setTooltip("Direct Monitor");
        directMonitorBtn.onClick = [this]()
        {
            channelRef.setDirectMonitor(directMonitorBtn.getToggleState());
        };
        addAndMakeVisible(directMonitorBtn);

        // Mute Button (Red)
        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff450a0a));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setTooltip("Mute Channel");
        muteBtn.onClick = [this]()
        {
            channelRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Disable Output / Route Button (Blue)
        disableOutputBtn.setClickingTogglesState(true);
        disableOutputBtn.setToggleState(true, juce::dontSendNotification);
        disableOutputBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        disableOutputBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentBlue());
        disableOutputBtn.setTooltip("Route Output to Mix");
        disableOutputBtn.onClick = [this]()
        {
            channelRef.setDisableOutput(!disableOutputBtn.getToggleState());
        };
        addAndMakeVisible(disableOutputBtn);

        // Phase Invert Button
        phaseInvertBtn.setClickingTogglesState(true);
        phaseInvertBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3139));
        phaseInvertBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
        phaseInvertBtn.setTooltip("Invert Phase (180 deg)");
        phaseInvertBtn.onClick = [this]()
        {
            channelRef.setPhaseInvert(phaseInvertBtn.getToggleState());
        };
        addAndMakeVisible(phaseInvertBtn);

        // Mono Button
        monoBtn.setClickingTogglesState(true);
        monoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3139));
        monoBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd97706));
        monoBtn.setTooltip("Force Mono Summing");
        monoBtn.onClick = [this]()
        {
            channelRef.setForceMono(monoBtn.getToggleState());
        };
        addAndMakeVisible(monoBtn);

        // VST Plugin Rack Button
        vstRackBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        vstRackBtn.setTooltip("Open Channel VST3 Plugin Rack");
        vstRackBtn.onClick = [this]() { openVstRackWindow(); };
        addAndMakeVisible(vstRackBtn);
    }

    void ChannelStrip::openVstRackWindow()
    {
        auto rackComp = std::make_unique<PluginRackDialog>(channelRef);

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(rackComp.release());
        opts.dialogTitle = "DSD Mixer - " + channelNameLabel.getText() + " VST3 Rack";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;

        opts.launchAsync();
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

        // Update plugin count on VST button
        int count = channelRef.getPluginRack().getNumPlugins();
        if (count > 0)
            vstRackBtn.setButtonText("[ VST (" + juce::String(count) + ") ]");
        else
            vstRackBtn.setButtonText("[ VST RACK ]");
    }

    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(3, 3);

        // 1. Top Section: OLED panel housing meter & badges
        auto oledArea = area.removeFromTop(74);
        auto meterCol = oledArea.removeFromLeft(34);
        topMeter.setBounds(meterCol.removeFromTop(52));
        dbfsReadout.setBounds(meterCol);

        oledArea.removeFromLeft(4);
        auto badgesCol = oledArea;
        directMonitorBtn.setBounds(badgesCol.removeFromTop(24).removeFromLeft(38));
        chNumberBadge.setBounds(badgesCol.removeFromTop(24).removeFromRight(46));

        area.removeFromTop(5);

        // 2. Buttons Row: MUTE and OUT
        auto btnRow = area.removeFromTop(26);
        const int btnW = (btnRow.getWidth() - 3) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(3);
        disableOutputBtn.setBounds(btnRow);

        area.removeFromTop(4);

        // 3. Utility row: Phase and Mono
        auto utilRow = area.removeFromTop(22);
        phaseInvertBtn.setBounds(utilRow.removeFromLeft(btnW));
        utilRow.removeFromLeft(3);
        monoBtn.setBounds(utilRow);

        area.removeFromTop(5);

        // 4. VST Rack Button
        vstRackBtn.setBounds(area.removeFromTop(24));

        area.removeFromTop(6);

        // 5. Bottom Section: Channel Name Label
        channelNameLabel.setBounds(area.removeFromBottom(26));

        area.removeFromBottom(5);

        // 6. Fader
        fader.setBounds(area);
    }

    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Authentic console strip background (matte grey)
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        // OLED Inset
        auto oledBounds = bounds.reduced(3.0f).removeFromTop(74.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(oledBounds, 4.0f);
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(oledBounds, 4.0f, 1.0f);

        // Bevel border
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
