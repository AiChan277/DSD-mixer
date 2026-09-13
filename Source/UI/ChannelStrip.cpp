#include "UI/ChannelStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/PluginRackDialog.h"
#include "DSP/GainProcessor.h"
#include "Audio/AudioInputSource.h"
#include "Audio/MultiDeviceManager.h"

namespace dsd
{
    ChannelStrip::ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager)
        : channelRef(channel),
          devMgrRef(deviceManager),
          topMeter(false)
    {
        // 1. Device input selector at the top
        inputDeviceSelector.setTextWhenNothingSelected("None");
        inputDeviceSelector.onBeforePopup = [this]()
        {
            refreshDeviceList();
        };
        inputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(inputDeviceSelector);
        refreshDeviceList();

        // 2. Channel Number Badge
        chNumberBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()), juce::dontSendNotification);
        chNumberBadge.setJustificationType(juce::Justification::centred);
        chNumberBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        chNumberBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff15263F));
        chNumberBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xff2563EB));
        chNumberBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(chNumberBadge);

        // 3. Fader dB Value Readout (in OLED card)
        gainReadout.setText(juce::String(channelRef.getFaderDb(), 1) + " dB", juce::dontSendNotification);
        gainReadout.setJustificationType(juce::Justification::centredRight);
        gainReadout.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        gainReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(gainReadout);

        // 4. Channel Name in OLED Screen (Editable)
        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);
        channelNameLabel.setJustificationType(juce::Justification::centred);
        channelNameLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        channelNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff16191F));
        channelNameLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff2F3542));
        channelNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        channelNameLabel.setEditable(true);
        channelNameLabel.setTooltip("Click to rename channel");
        channelNameLabel.onTextChange = [this]()
        {
            channelRef.setName(channelNameLabel.getText().toStdString());
        };
        addAndMakeVisible(channelNameLabel);

        // 5. Mini Top Meter & Peak LED
        topMeter.setClipResetCallback([this]()
        {
            channelRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(topMeter);

        // 6. dBFS Numeric Readout (bottom row of right section in OLED card)
        dbfsReadout.setText("-oo dBFS", juce::dontSendNotification);
        dbfsReadout.setJustificationType(juce::Justification::centred);
        dbfsReadout.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        dbfsReadout.setColour(juce::Label::backgroundColourId, juce::Colour(0xff101216));
        dbfsReadout.setColour(juce::Label::outlineColourId, juce::Colour(0xff222630));
        dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        addAndMakeVisible(dbfsReadout);

        // 7. Functional Console Buttons
        setupButtons();

        // 8. Broadcast Fader
        fader.setValue(channelRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            channelRef.setFaderDb(db);
            if (db <= -59.5f)
                gainReadout.setText("-inf dB", juce::dontSendNotification);
            else
                gainReadout.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
        });
        addAndMakeVisible(fader);
    }

    ChannelStrip::~ChannelStrip()
    {
        if (activeRackWindow != nullptr)
        {
            activeRackWindow->setVisible(false);
            delete activeRackWindow.getComponent();
        }
    }

    void ChannelStrip::refreshDeviceList()
    {
        inputDeviceSelector.clear(juce::dontSendNotification);

        // 1. None option
        inputDeviceSelector.addItem("None", 1);

        // 2. Direct Hardware Inputs (Zero-Latency, 100% Glitch-Free)
        inputDeviceSelector.addSectionHeading("[ Direct Hardware Inputs ]");
        inputDeviceSelector.addItem("Primary In 1 (Mic / L) [Clean Direct]", 10);
        inputDeviceSelector.addItem("Primary In 2 (R) [Clean Direct]", 11);
        inputDeviceSelector.addItem("Primary In 1+2 (Stereo) [Clean Direct]", 12);

        // 3. Application Audio Capture (OBS Style Process Loopback)
        runningApps = WindowAudioCapture::getRunningApplications();
        if (!runningApps.empty())
        {
            inputDeviceSelector.addSectionHeading("[ Application Audio (OBS Style) ]");
            for (size_t i = 0; i < runningApps.size(); ++i)
            {
                juce::String label = runningApps[i].appName;
                if (runningApps[i].windowTitle.isNotEmpty())
                    label += " (" + runningApps[i].windowTitle.substring(0, 25) + ")";
                inputDeviceSelector.addItem(label, 500 + static_cast<int>(i));
            }
        }

        // 4. Real Windows Audio Input Devices (WASAPI)
        auto winInputs = MultiDeviceManager::getInstance().getAvailableInputDevices();
        if (!winInputs.isEmpty())
        {
            inputDeviceSelector.addSectionHeading("[ Windows Audio Devices ]");
            for (int i = 0; i < winInputs.size(); ++i)
            {
                inputDeviceSelector.addItem(winInputs[i], 100 + i);
            }
        }

        // 5. Test Tone Generators
        inputDeviceSelector.addSectionHeading("[ Test Tone Generators ]");
        inputDeviceSelector.addItem("Sine Wave (1 kHz)", 50);
        inputDeviceSelector.addItem("Pink Noise", 51);

        // Select current device
        const juce::String currentDev = channelRef.getInputDeviceName();
        if (currentDev.isEmpty() || currentDev == "None")
        {
            inputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Primary In 1") || currentDev.containsIgnoreCase("Mic / L"))
        {
            inputDeviceSelector.setSelectedId(10, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Primary In 2"))
        {
            inputDeviceSelector.setSelectedId(11, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Stereo") && currentDev.containsIgnoreCase("Primary"))
        {
            inputDeviceSelector.setSelectedId(12, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Sine"))
        {
            inputDeviceSelector.setSelectedId(50, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Noise"))
        {
            inputDeviceSelector.setSelectedId(51, juce::dontSendNotification);
        }
        else
        {
            int foundId = -1;
            // Check running apps first
            for (size_t i = 0; i < runningApps.size(); ++i)
            {
                if (runningApps[i].appName.equalsIgnoreCase(currentDev))
                {
                    foundId = 500 + static_cast<int>(i);
                    break;
                }
            }
            if (foundId < 0)
            {
                for (int i = 0; i < winInputs.size(); ++i)
                {
                    if (winInputs[i] == currentDev)
                    {
                        foundId = 100 + i;
                        break;
                    }
                }
            }

            if (foundId > 0)
                inputDeviceSelector.setSelectedId(foundId, juce::dontSendNotification);
            else
                inputDeviceSelector.setText(currentDev, juce::dontSendNotification);
        }
    }

    void ChannelStrip::updateUIFromChannel()
    {
        // 1. Fader & Gain Readout
        const float db = channelRef.getFaderDb();
        fader.setValue(db, juce::dontSendNotification);
        if (db <= -59.5f)
            gainReadout.setText("-inf dB", juce::dontSendNotification);
        else
            gainReadout.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);

        // 2. Channel Name
        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);

        // 3. Broadcast ON / OFF Buttons
        const bool isMuted = channelRef.getMute() || channelRef.getDisableOutput();
        onBtn.setToggleState(!isMuted, juce::dontSendNotification);
        muteBtn.setToggleState(isMuted, juce::dontSendNotification);

        // 4. Sub-function Utility Buttons
        directMonitorBtn.setToggleState(channelRef.getDirectMonitor(), juce::dontSendNotification);
        phaseInvertBtn.setToggleState(channelRef.getPhaseInvert(), juce::dontSendNotification);
        monoBtn.setToggleState(channelRef.getForceMono(), juce::dontSendNotification);

        // 4. Input Selector Dropdown
        refreshDeviceList();
    }

    void ChannelStrip::onDeviceSelected()
    {
        const int id = inputDeviceSelector.getSelectedId();

        if (id <= 1)
        {
            channelRef.setInputChannelIndex(-1);
            channelRef.setInputDeviceName("None");
            channelRef.setInputSource(std::make_unique<NullInputSource>());
        }
        else if (id == 10)
        {
            // Primary In 1 (Mic / L centered to stereo) - Direct Hardware
            channelRef.setInputChannelIndex(0);
            channelRef.setInputDeviceName("Primary In 1 (Mic / L)");
            channelRef.setInputSource(std::make_unique<HardwareInputSource>(0, -1));
        }
        else if (id == 11)
        {
            // Primary In 2 (Right channel centered to stereo) - Direct Hardware
            channelRef.setInputChannelIndex(1);
            channelRef.setInputDeviceName("Primary In 2 (R)");
            channelRef.setInputSource(std::make_unique<HardwareInputSource>(1, -1));
        }
        else if (id == 12)
        {
            // Primary In 1+2 (Stereo pair) - Direct Hardware
            channelRef.setInputChannelIndex(0);
            channelRef.setInputDeviceName("Primary In 1+2 (Stereo)");
            channelRef.setInputSource(std::make_unique<HardwareInputSource>(0, 1));
        }
        else if (id == 50)
        {
            channelRef.setInputChannelIndex(0);
            channelRef.setInputDeviceName("Sine Wave (1 kHz)");
            channelRef.setInputSource(std::make_unique<SineInputSource>(1000.0f));
        }
        else if (id == 51)
        {
            channelRef.setInputChannelIndex(0);
            channelRef.setInputDeviceName("Pink Noise");
            channelRef.setInputSource(std::make_unique<NoiseInputSource>());
        }
        else if (id >= 500 && id < 500 + static_cast<int>(runningApps.size()))
        {
            // Application Audio Capture (OBS Style Process Loopback)
            const size_t appIdx = static_cast<size_t>(id - 500);
            const auto& app = runningApps[appIdx];
            channelRef.setInputChannelIndex(static_cast<int>(appIdx));
            channelRef.setInputDeviceName(app.appName.toStdString());
            channelRef.setInputSource(std::make_unique<WindowAudioCapture>(app.processId, app.appName));
        }
        else if (id >= 100 && id < 500)
        {
            const juce::String selectedName = inputDeviceSelector.getText();
            channelRef.setInputChannelIndex(id - 100);
            channelRef.setInputDeviceName(selectedName.toStdString());

            const juce::String primaryName = MultiDeviceManager::getInstance().getPrimaryInputDeviceName();
            if (primaryName.isNotEmpty() && selectedName.equalsIgnoreCase(primaryName))
            {
                // Seamlessly use Direct Hardware Input if this device is the default host mic!
                channelRef.setInputSource(std::make_unique<HardwareInputSource>(0, -1));
            }
            else
            {
                auto source = MultiDeviceManager::getInstance().createInputSourceFor(selectedName);
                channelRef.setInputSource(std::move(source));
            }
        }
    }

    void ChannelStrip::setupButtons()
    {
        // 1. Large Broadcast "ON" Button
        onBtn.setClickingTogglesState(true);
        onBtn.setToggleState(true, juce::dontSendNotification);
        onBtn.setTooltip("Channel ON (Live to mix)");
        onBtn.onClick = [this]()
        {
            const bool isOn = onBtn.getToggleState();
            channelRef.setDisableOutput(!isOn);
            channelRef.setMute(!isOn);
            muteBtn.setToggleState(!isOn, juce::dontSendNotification);
        };
        addAndMakeVisible(onBtn);

        // 2. Broadcast "OFF" (Mute) Button
        muteBtn.setClickingTogglesState(true);
        muteBtn.setToggleState(false, juce::dontSendNotification);
        muteBtn.setTooltip("Channel OFF (Mute)");
        muteBtn.onClick = [this]()
        {
            const bool isOff = muteBtn.getToggleState();
            channelRef.setMute(isOff);
            channelRef.setDisableOutput(isOff);
            onBtn.setToggleState(!isOff, juce::dontSendNotification);
        };
        addAndMakeVisible(muteBtn);

        // 3. Compact Sub-function Utility Buttons
        vstRackBtn.setButtonText("VST");
        vstRackBtn.setTooltip("Open Channel VST3 Plugin Rack");
        vstRackBtn.onClick = [this]() { openVstRackWindow(); };
        addAndMakeVisible(vstRackBtn);

        directMonitorBtn.setClickingTogglesState(true);
        directMonitorBtn.setTooltip("Direct Monitor (Pre-fader listen)");
        directMonitorBtn.onClick = [this]()
        {
            channelRef.setDirectMonitor(directMonitorBtn.getToggleState());
        };
        addAndMakeVisible(directMonitorBtn);

        phaseInvertBtn.setButtonText("PH");
        phaseInvertBtn.setClickingTogglesState(true);
        phaseInvertBtn.setTooltip("Invert Phase (180 deg)");
        phaseInvertBtn.onClick = [this]()
        {
            channelRef.setPhaseInvert(phaseInvertBtn.getToggleState());
        };
        addAndMakeVisible(phaseInvertBtn);

        monoBtn.setClickingTogglesState(true);
        monoBtn.setTooltip("Force Mono Summing");
        monoBtn.onClick = [this]()
        {
            channelRef.setForceMono(monoBtn.getToggleState());
        };
        addAndMakeVisible(monoBtn);
    }

    void ChannelStrip::openVstRackWindow()
    {
        if (activeRackWindow != nullptr)
        {
            activeRackWindow->toFront(true);
            return;
        }

        activeRackWindow = new PluginRackWindow("DSD Mixer - " + channelNameLabel.getText() + " VST3 Rack", channelRef);
    }

    void ChannelStrip::updateMeterFromAudio()
    {
        const auto& mv = channelRef.getMeterValues();
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float peakMax = juce::jmax(pL, pR);
        const float hL = mv.peakHoldL.load(std::memory_order_relaxed);
        const float hR = mv.peakHoldR.load(std::memory_order_relaxed);
        const bool clip = mv.clipped.load(std::memory_order_relaxed);

        topMeter.setMeterValues(pL, pR, hL, hR, clip);

        if (peakMax <= 0.001f)
        {
            dbfsReadout.setText("-oo dBFS", juce::dontSendNotification);
            dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        }
        else
        {
            const float db = GainProcessor::linearToDb(peakMax);
            dbfsReadout.setText(juce::String(db, 1) + " dBFS", juce::dontSendNotification);
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
            vstRackBtn.setButtonText("VST " + juce::String(count));
        else
            vstRackBtn.setButtonText("VST");
    }

    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Device input selector at the top (height: 22px)
        inputDeviceSelector.setBounds(area.removeFromTop(22));

        area.removeFromTop(4);

        // 2. Channel OLED Display Area (Height: 88px) - User Sketch Layout
        auto oledArea = area.removeFromTop(88).reduced(3, 3);

        // Left Column: Peak LED on top + tall vertical level meter bar
        auto meterCol = oledArea.removeFromLeft(22);
        topMeter.setBounds(meterCol);

        oledArea.removeFromLeft(4); // Gap between meter and right section

        // Right Section: 3 stacked tiers
        auto rightSection = oledArea;

        // Tier 1 (Top): CH Badge + Fader dB Readout
        auto tier1 = rightSection.removeFromTop(22);
        chNumberBadge.setBounds(tier1.removeFromLeft(40));
        tier1.removeFromLeft(2);
        gainReadout.setBounds(tier1);

        rightSection.removeFromTop(3);

        // Tier 2 (Middle): Channel Name (Label) - Full width
        channelNameLabel.setBounds(rightSection.removeFromTop(26));

        rightSection.removeFromTop(3);

        // Tier 3 (Bottom): dBFS Numeric Readout Meter - Full width
        dbfsReadout.setBounds(rightSection.removeFromTop(24));

        area.removeFromTop(5);

        // 3. Sub-function Utility Buttons in 2x2 Grid Layout
        auto btnGridArea = area.removeFromTop(48);

        // Row 1: [ VST ] [ DM ]
        auto btnRow1 = btnGridArea.removeFromTop(22);
        const int halfBtnW = (btnRow1.getWidth() - 4) / 2;
        vstRackBtn.setBounds(btnRow1.removeFromLeft(halfBtnW));
        btnRow1.removeFromLeft(4);
        directMonitorBtn.setBounds(btnRow1);

        btnGridArea.removeFromTop(4);

        // Row 2: [ PH ] [ MONO ]
        auto btnRow2 = btnGridArea.removeFromTop(22);
        phaseInvertBtn.setBounds(btnRow2.removeFromLeft(halfBtnW));
        btnRow2.removeFromLeft(4);
        monoBtn.setBounds(btnRow2);

        area.removeFromTop(4);

        // 4. Large Broadcast Buttons at Bottom
        area.removeFromBottom(4); // Extra cushion at bottom edge
        auto botArea = area.removeFromBottom(62);
        onBtn.setBounds(botArea.removeFromTop(32).reduced(2, 0));
        botArea.removeFromTop(4);
        muteBtn.setBounds(botArea.removeFromTop(26).reduced(2, 0));

        area.removeFromBottom(5); // Cushion between fader and ON button

        // 5. Hardware Broadcast Fader fills remainder with full vertical travel
        fader.setBounds(area);
    }

    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Authentic console strip body (clean light matte grey chassis panel)
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        // 2. OLED Display recessed cutout (height: 88px)
        auto oledBounds = juce::Rectangle<float>(bounds.getX() + 4.0f,
                                                 bounds.getY() + 4.0f + 22.0f + 4.0f,
                                                 bounds.getWidth() - 8.0f,
                                                 88.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(oledBounds, 4.0f);
        g.setColour(juce::Colour(0xff3B3F4A));
        g.drawRoundedRectangle(oledBounds, 4.0f, 1.0f);

        // 3. Bevel border around entire channel strip
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
