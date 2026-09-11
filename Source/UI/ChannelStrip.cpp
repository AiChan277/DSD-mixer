#include "UI/ChannelStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/PluginRackDialog.h"
#include "DSP/GainProcessor.h"
#include "Audio/AudioInputSource.h"

namespace dsd
{
    ChannelStrip::ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager)
        : channelRef(channel),
          devMgrRef(deviceManager),
          topMeter(false)
    {
        addAndMakeVisible(inputDeviceSelector);
        inputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        refreshDeviceList();

        chNumberBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()), juce::dontSendNotification);
        chNumberBadge.setJustificationType(juce::Justification::centred);
        chNumberBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        chNumberBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentBlue().withAlpha(0.35f));
        chNumberBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentBlue());
        chNumberBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(chNumberBadge);

        dbfsReadout.setText("-oo", juce::dontSendNotification);
        dbfsReadout.setJustificationType(juce::Justification::centred);
        dbfsReadout.setFont(juce::FontOptions(10.0f));
        dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        addAndMakeVisible(dbfsReadout);

        topMeter.setClipResetCallback([this]()
        {
            channelRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(topMeter);

        setupButtons();

        gainReadout.setText(juce::String(channelRef.getFaderDb(), 1) + " dB", juce::dontSendNotification);
        gainReadout.setJustificationType(juce::Justification::centred);
        gainReadout.setFont(juce::FontOptions(11.0f));
        gainReadout.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        gainReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(gainReadout);

        fader.setValue(channelRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            channelRef.setFaderDb(db);
            gainReadout.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        });
        addAndMakeVisible(fader);

        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);
        channelNameLabel.setJustificationType(juce::Justification::centred);
        channelNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        channelNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        channelNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getConsoleBevel());
        channelNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        channelNameLabel.setEditable(true);
        channelNameLabel.onTextChange = [this]()
        {
            channelRef.setName(channelNameLabel.getText().toStdString());
        };
        addAndMakeVisible(channelNameLabel);
    }

    void ChannelStrip::refreshDeviceList()
    {
        inputDeviceSelector.clear(juce::dontSendNotification);
        inputDeviceSelector.addItem("None", 1);
        
        if (auto* device = devMgrRef.getCurrentAudioDevice())
        {
            auto inputNames = device->getInputChannelNames();
            for (int i = 0; i < inputNames.size(); i += 2)
            {
                int id = i / 2 + 2;
                juce::String label;
                if (i + 1 < inputNames.size())
                    label = inputNames[i] + " / " + inputNames[i + 1];
                else
                    label = inputNames[i];
                inputDeviceSelector.addItem(label, id);
            }
        }
        
        int currentId = 1;
        int currentCh = channelRef.getInputChannelIndex();
        if (currentCh >= 0)
            currentId = (currentCh / 2) + 2;
        inputDeviceSelector.setSelectedId(currentId, juce::dontSendNotification);
    }

    void ChannelStrip::onDeviceSelected()
    {
        int selected = inputDeviceSelector.getSelectedId();
        if (selected <= 1)
        {
            channelRef.setInputChannelIndex(-1);
            channelRef.setInputDeviceName("None");
            channelRef.setInputSource(std::make_unique<NullInputSource>());
        }
        else
        {
            int pairIndex = selected - 2;
            int startCh = pairIndex * 2;
            channelRef.setInputChannelIndex(startCh);
            channelRef.setInputDeviceName(inputDeviceSelector.getText().toStdString());
            channelRef.setInputSource(std::make_unique<HardwareInputSource>(startCh, startCh + 1));
        }
    }

    void ChannelStrip::setupButtons()
    {
        directMonitorBtn.setClickingTogglesState(true);
        directMonitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        directMonitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        directMonitorBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        directMonitorBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        directMonitorBtn.setTooltip("Direct Monitor");
        directMonitorBtn.onClick = [this]() { channelRef.setDirectMonitor(directMonitorBtn.getToggleState()); };
        addAndMakeVisible(directMonitorBtn);

        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8B2020));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        muteBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        muteBtn.setTooltip("Mute Channel");
        muteBtn.onClick = [this]() { channelRef.setMute(muteBtn.getToggleState()); };
        addAndMakeVisible(muteBtn);

        disableOutputBtn.setClickingTogglesState(true);
        disableOutputBtn.setToggleState(true, juce::dontSendNotification);
        disableOutputBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e3a5a));
        disableOutputBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentBlue());
        disableOutputBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        disableOutputBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        disableOutputBtn.setTooltip("Route Output to Mix");
        disableOutputBtn.onClick = [this]() { channelRef.setDisableOutput(!disableOutputBtn.getToggleState()); };
        addAndMakeVisible(disableOutputBtn);

        phaseInvertBtn.setButtonText("PH");
        phaseInvertBtn.setClickingTogglesState(true);
        phaseInvertBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a4a20));
        phaseInvertBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffD97706));
        phaseInvertBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        phaseInvertBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        phaseInvertBtn.setTooltip("Invert Phase (180 deg)");
        phaseInvertBtn.onClick = [this]() { channelRef.setPhaseInvert(phaseInvertBtn.getToggleState()); };
        addAndMakeVisible(phaseInvertBtn);

        monoBtn.setClickingTogglesState(true);
        monoBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5a4a20));
        monoBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffD97706));
        monoBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        monoBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        monoBtn.setTooltip("Force Mono Summing");
        monoBtn.onClick = [this]() { channelRef.setForceMono(monoBtn.getToggleState()); };
        addAndMakeVisible(monoBtn);

        vstRackBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        vstRackBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        vstRackBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
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

        int count = channelRef.getPluginRack().getNumPlugins();
        if (count > 0)
            vstRackBtn.setButtonText("[ VST (" + juce::String(count) + ") ]");
        else
            vstRackBtn.setButtonText("VST RACK");
    }

    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(3, 3);
        
        inputDeviceSelector.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        auto oledArea = area.removeFromTop(70);
        auto meterCol = oledArea.removeFromLeft(28);
        topMeter.setBounds(meterCol.reduced(2, 2));

        auto textCol = oledArea;
        dbfsReadout.setBounds(textCol.removeFromTop(35));
        chNumberBadge.setBounds(textCol.reduced(2, 2));

        area.removeFromTop(4);

        directMonitorBtn.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);

        auto btnRow1 = area.removeFromTop(26);
        muteBtn.setBounds(btnRow1.removeFromLeft(btnRow1.getWidth() / 2 - 2));
        btnRow1.removeFromLeft(4);
        disableOutputBtn.setBounds(btnRow1);
        area.removeFromTop(4);

        auto btnRow2 = area.removeFromTop(22);
        phaseInvertBtn.setBounds(btnRow2.removeFromLeft(btnRow2.getWidth() / 2 - 2));
        btnRow2.removeFromLeft(4);
        monoBtn.setBounds(btnRow2);
        area.removeFromTop(4);

        vstRackBtn.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);

        gainReadout.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);

        channelNameLabel.setBounds(area.removeFromBottom(26));
        area.removeFromBottom(4);
        fader.setBounds(area);
    }

    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        auto oledBounds = juce::Rectangle<float>(bounds.getX() + 3.0f, bounds.getY() + 3.0f + 20.0f + 4.0f, bounds.getWidth() - 6.0f, 70.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(oledBounds, 4.0f);

        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
}
