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
        inputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(inputDeviceSelector);
        refreshDeviceList();

        // 2. Channel Number Badge
        chNumberBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()), juce::dontSendNotification);
        chNumberBadge.setJustificationType(juce::Justification::centred);
        chNumberBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        chNumberBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff15263F));
        chNumberBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xff2563EB));
        chNumberBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(chNumberBadge);

        // 3. dBFS Numeric Readout
        dbfsReadout.setText("-oo", juce::dontSendNotification);
        dbfsReadout.setJustificationType(juce::Justification::centred);
        dbfsReadout.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        dbfsReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        addAndMakeVisible(dbfsReadout);

        // 4. Mini Top Meter & Peak LED
        topMeter.setClipResetCallback([this]()
        {
            channelRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(topMeter);

        // 5. Functional Console Buttons
        setupButtons();

        // 6. Fader dB Value Readout
        gainReadout.setText(juce::String(channelRef.getFaderDb(), 1) + " dB", juce::dontSendNotification);
        gainReadout.setJustificationType(juce::Justification::centred);
        gainReadout.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        gainReadout.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        gainReadout.setColour(juce::Label::outlineColourId, juce::Colour(0xff4B505C));
        gainReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(gainReadout);

        // 7. Broadcast Fader
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

        // 8. Channel Name Bottom Label (Editable)
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
        // Direct Monitor (DM) Button - Illuminated Amber on toggle
        directMonitorBtn.setClickingTogglesState(true);
        directMonitorBtn.setTooltip("Direct Monitor (Pre-fader listen)");
        directMonitorBtn.onClick = [this]()
        {
            channelRef.setDirectMonitor(directMonitorBtn.getToggleState());
        };
        addAndMakeVisible(directMonitorBtn);

        // Mute Button - Illuminated Amber on toggle
        muteBtn.setClickingTogglesState(true);
        muteBtn.setTooltip("Mute Channel");
        muteBtn.onClick = [this]()
        {
            channelRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Output Route (OUT) Button - Illuminated Amber on toggle, starts ON
        disableOutputBtn.setClickingTogglesState(true);
        disableOutputBtn.setToggleState(true, juce::dontSendNotification);
        disableOutputBtn.setTooltip("Route Channel to Mix Outputs");
        disableOutputBtn.onClick = [this]()
        {
            channelRef.setDisableOutput(!disableOutputBtn.getToggleState());
        };
        addAndMakeVisible(disableOutputBtn);

        // Phase Invert (PH) Button - Illuminated Amber on toggle
        phaseInvertBtn.setButtonText("PH");
        phaseInvertBtn.setClickingTogglesState(true);
        phaseInvertBtn.setTooltip("Invert Phase (180 deg)");
        phaseInvertBtn.onClick = [this]()
        {
            channelRef.setPhaseInvert(phaseInvertBtn.getToggleState());
        };
        addAndMakeVisible(phaseInvertBtn);

        // Mono Sum (MONO) Button - Illuminated Amber on toggle
        monoBtn.setClickingTogglesState(true);
        monoBtn.setTooltip("Force Mono Summing");
        monoBtn.onClick = [this]()
        {
            channelRef.setForceMono(monoBtn.getToggleState());
        };
        addAndMakeVisible(monoBtn);

        // VST Plugin Rack Button
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
        const float peakMax = juce::jmax(pL, pR);
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
            vstRackBtn.setButtonText("VST RACK");
    }

    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Device input selector at the top (neat 22px height)
        inputDeviceSelector.setBounds(area.removeFromTop(22));

        area.removeFromTop(4);

        // 2. Channel OLED Display Area (Height: 68px)
        auto oledArea = area.removeFromTop(68);
        auto meterCol = oledArea.removeFromLeft(30);
        topMeter.setBounds(meterCol.reduced(2, 2));

        oledArea.removeFromLeft(3);
        auto textCol = oledArea;
        chNumberBadge.setBounds(textCol.removeFromTop(26).reduced(1, 1));
        textCol.removeFromTop(2);
        dbfsReadout.setBounds(textCol.removeFromTop(24).reduced(1, 1));

        area.removeFromTop(5);

        // 3. Direct Monitor Button (Full Width)
        directMonitorBtn.setBounds(area.removeFromTop(24));

        area.removeFromTop(4);

        // 4. MUTE and OUT Buttons Row
        auto row1 = area.removeFromTop(26);
        const int halfW = (row1.getWidth() - 4) / 2;
        muteBtn.setBounds(row1.removeFromLeft(halfW));
        row1.removeFromLeft(4);
        disableOutputBtn.setBounds(row1);

        area.removeFromTop(4);

        // 5. PH and MONO Buttons Row
        auto row2 = area.removeFromTop(22);
        phaseInvertBtn.setBounds(row2.removeFromLeft(halfW));
        row2.removeFromLeft(4);
        monoBtn.setBounds(row2);

        area.removeFromTop(4);

        // 6. VST RACK Button
        vstRackBtn.setBounds(area.removeFromTop(24));

        area.removeFromTop(5);

        // 7. Fader dB Value Readout
        gainReadout.setBounds(area.removeFromTop(18));

        area.removeFromTop(5);

        // 8. Channel Name Label at Bottom
        channelNameLabel.setBounds(area.removeFromBottom(26));

        area.removeFromBottom(5);

        // 9. Broadcast Fader
        fader.setBounds(area);
    }

    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Authentic console strip body (clean light matte grey chassis panel)
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        // 2. OLED Display recessed cutout
        auto oledBounds = juce::Rectangle<float>(bounds.getX() + 4.0f,
                                                 bounds.getY() + 4.0f + 22.0f + 4.0f,
                                                 bounds.getWidth() - 8.0f,
                                                 68.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(oledBounds, 4.0f);
        g.setColour(juce::Colour(0xff3B3F4A));
        g.drawRoundedRectangle(oledBounds, 4.0f, 1.0f);

        // 3. Bevel border around entire channel strip
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
