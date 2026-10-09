#include "UI/ChannelStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/PluginRackDialog.h"
#include "DSP/GainProcessor.h"
#include "Audio/AudioInputSource.h"
#include "Audio/MultiDeviceManager.h"
#include "Audio/AudioTypes.h"

namespace dsd
{
    // =========================================================================
    // Constructor
    // =========================================================================
    ChannelStrip::ChannelStrip(AudioChannel& channel, juce::AudioDeviceManager& deviceManager)
        : channelRef(channel),
          devMgrRef(deviceManager),
          topMeter(false, true)
    {
        // 1. Input device selector
        inputDeviceSelector.setTextWhenNothingSelected("None");
        inputDeviceSelector.onBeforePopup = [this]() { refreshDeviceList(); };
        inputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(inputDeviceSelector);
        refreshDeviceList();

        // 2. CH badge (in OLED top-center)
        chNumberBadge.setText(juce::String::formatted("CH %02d", channelRef.getChannelID()),
                              juce::dontSendNotification);
        chNumberBadge.setJustificationType(juce::Justification::centred);
        chNumberBadge.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        chNumberBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff122238));
        chNumberBadge.setColour(juce::Label::outlineColourId,    juce::Colour(0xff2563EB));
        chNumberBadge.setColour(juce::Label::textColourId,       juce::Colour(0xffD0E4FF));
        addAndMakeVisible(chNumberBadge);

        // 3. Fader dB readout (in OLED, top-right)
        faderReadout.setJustificationType(juce::Justification::centredRight);
        faderReadout.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        faderReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        faderReadout.setTooltip("Channel fader level (dB)");
        {
            const float fd = channelRef.getFaderDb();
            faderReadout.setText(
                fd <= -59.5f ? "-inf dB" : (juce::String(fd, 1) + " dB"),
                juce::dontSendNotification);
        }
        addAndMakeVisible(faderReadout);

        // 4. Channel name label (in OLED, middle pill)
        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);
        channelNameLabel.setJustificationType(juce::Justification::centred);
        channelNameLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        channelNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff14171E));
        channelNameLabel.setColour(juce::Label::outlineColourId,    juce::Colour(0xff2B303C));
        channelNameLabel.setColour(juce::Label::textColourId,       DSDLookAndFeel::getTextOnOled());
        channelNameLabel.setEditable(true);
        channelNameLabel.setTooltip("Click to rename channel");
        channelNameLabel.onTextChange = [this]()
        {
            channelRef.setName(channelNameLabel.getText().toStdString());
        };
        addAndMakeVisible(channelNameLabel);

        // 5. PRE-GAIN numeric dBFS readout (in OLED, lower row)
        preGainDbfsReadout.setText("IN: -oo dBFS", juce::dontSendNotification);
        preGainDbfsReadout.setJustificationType(juce::Justification::centred);
        preGainDbfsReadout.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        preGainDbfsReadout.setColour(juce::Label::textColourId, juce::Colour(0xffC0C5CC));
        preGainDbfsReadout.setTooltip("Pre-gain peak level (dBFS)");
        addAndMakeVisible(preGainDbfsReadout);

        // 6. PRE-GAIN meter (Stage A) - horizontal segmented LED bar
        topMeter.setClipResetCallback([this]()
        {
            channelRef.getMeterInput().resetClip();
        });
        topMeter.setTooltip("PRE-GAIN: raw input before gain and fader");
        addAndMakeVisible(topMeter);

        // 7. GAIN knob
        gainKnob.setValue(channelRef.getGainDb(), false);
        gainKnob.formatValue = [](double v)
        {
            if (v <= -59.5)
                return juce::String("-inf");
            return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
        };
        gainKnob.onValueChanged = [this](double db)
        {
            channelRef.setGainDb(static_cast<float>(db));
        };
        gainKnob.setTooltip("GAIN: channel pre-gain (-60 dB to +24 dB)");
        addAndMakeVisible(gainKnob);

        // 8. PAN knob
        panKnob.setBipolar(true);
        panKnob.setValue(static_cast<double>(channelRef.getPan()), false);
        panKnob.formatValue = [](double v)
        {
            if (std::abs(v) < 0.02)
                return juce::String("C");
            if (v < 0.0)
                return "L " + juce::String(static_cast<int>(std::round(-v * 100.0)));
            return "R " + juce::String(static_cast<int>(std::round(v * 100.0)));
        };
        panKnob.onValueChanged = [this](double panVal)
        {
            channelRef.setPan(static_cast<float>(panVal));
        };
        panKnob.setTooltip("PAN: constant-power stereo pan (-1 = L, 0 = C, +1 = R)");
        addAndMakeVisible(panKnob);

        // 9. All buttons
        setupButtons();

        // 10. Hardware fader
        fader.setValue(channelRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            channelRef.setFaderDb(db);
            faderReadout.setText(
                db <= -59.5f ? "-inf dB" : (juce::String(db, 1) + " dB"),
                juce::dontSendNotification);
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

    // =========================================================================
    // Device list
    // =========================================================================
    void ChannelStrip::refreshDeviceList()
    {
        inputDeviceSelector.clear(juce::dontSendNotification);

        inputDeviceSelector.addItem("None", 1);

        inputDeviceSelector.addSectionHeading("[ Direct Hardware Inputs ]");
        inputDeviceSelector.addItem("Primary In 1 (Mic / L) [Clean Direct]", 10);
        inputDeviceSelector.addItem("Primary In 2 (R) [Clean Direct]",        11);
        inputDeviceSelector.addItem("Primary In 1+2 (Stereo) [Clean Direct]", 12);

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

        auto winInputs = MultiDeviceManager::getInstance().getAvailableInputDevices();
        if (!winInputs.isEmpty())
        {
            inputDeviceSelector.addSectionHeading("[ Windows Audio Devices ]");
            for (int i = 0; i < winInputs.size(); ++i)
                inputDeviceSelector.addItem(winInputs[i], 100 + i);
        }

        inputDeviceSelector.addSectionHeading("[ Test Tone Generators ]");
        inputDeviceSelector.addItem("Sine Wave (1 kHz)", 50);
        inputDeviceSelector.addItem("Pink Noise",        51);

        const juce::String cur = channelRef.getInputDeviceName();
        if (cur.isEmpty() || cur == "None")
            inputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
        else if (cur.containsIgnoreCase("Mic / L") || cur.containsIgnoreCase("Primary In 1"))
            inputDeviceSelector.setSelectedId(10, juce::dontSendNotification);
        else if (cur.containsIgnoreCase("Primary In 2"))
            inputDeviceSelector.setSelectedId(11, juce::dontSendNotification);
        else if (cur.containsIgnoreCase("Stereo") && cur.containsIgnoreCase("Primary"))
            inputDeviceSelector.setSelectedId(12, juce::dontSendNotification);
        else if (cur.containsIgnoreCase("Sine"))
            inputDeviceSelector.setSelectedId(50, juce::dontSendNotification);
        else if (cur.containsIgnoreCase("Noise"))
            inputDeviceSelector.setSelectedId(51, juce::dontSendNotification);
        else
        {
            int foundId = -1;
            for (size_t i = 0; i < runningApps.size(); ++i)
            {
                if (runningApps[i].appName.equalsIgnoreCase(cur))
                    { foundId = 500 + static_cast<int>(i); break; }
            }
            if (foundId < 0)
            {
                for (int i = 0; i < winInputs.size(); ++i)
                {
                    if (winInputs[i] == cur)
                        { foundId = 100 + i; break; }
                }
            }
            if (foundId > 0)
                inputDeviceSelector.setSelectedId(foundId, juce::dontSendNotification);
            else
                inputDeviceSelector.setText(cur, juce::dontSendNotification);
        }
    }

    // =========================================================================
    // Sync UI from channel state
    // =========================================================================
    void ChannelStrip::updateUIFromChannel()
    {
        gainKnob.setValue(channelRef.getGainDb(), false);
        panKnob.setValue(static_cast<double>(channelRef.getPan()), false);

        const float fd = channelRef.getFaderDb();
        fader.setValue(fd, juce::dontSendNotification);
        faderReadout.setText(
            fd <= -59.5f ? "-inf dB" : (juce::String(fd, 1) + " dB"),
            juce::dontSendNotification);

        channelNameLabel.setText(channelRef.getName(), juce::dontSendNotification);

        // ON / OFF (mutually exclusive, independent of MUTE)
        const bool isOn = !channelRef.getDisableOutput();
        onBtn.setToggled(isOn, false);
        offBtn.setToggled(!isOn, false);

        // Console buttons
        muteBtn.setToggled(channelRef.getMute(), false);
        directMonitorBtn.setToggled(channelRef.getDirectMonitor(), false);
        phaseInvertBtn.setToggled(channelRef.getPhaseInvert(), false);
        monoBtn.setToggled(channelRef.getForceMono(), false);
        pflBtn.setToggled(channelRef.getPfl(), false);
        hpfBtn.setToggled(channelRef.getHpfEnabled(), false);
        padBtn.setToggled(channelRef.getPadEnabled(), false);
    }

    // =========================================================================
    // Device selection
    // =========================================================================
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
                channelRef.setInputSource(std::make_unique<HardwareInputSource>(0, -1));
            }
            else
            {
                auto source = MultiDeviceManager::getInstance().createInputSourceFor(selectedName);
                channelRef.setInputSource(std::move(source));
            }
        }
    }

    // =========================================================================
    // Button setup
    // =========================================================================
    void ChannelStrip::setupButtons()
    {
        // ── ON / OFF buttons ─────────────────────────────────────────────
        const bool isOn = !channelRef.getDisableOutput();
        onBtn.setToggleMode(false);
        onBtn.setToggled(isOn, false);
        onBtn.setTooltip("ON: activates channel output. Uses supplied illuminated button.");
        onBtn.onClick = [this]()
        {
            channelRef.setDisableOutput(false);
            onBtn.setToggled(true, false);
            offBtn.setToggled(false, false);
        };
        addAndMakeVisible(onBtn);

        offBtn.setToggleMode(false);
        offBtn.setToggled(!isOn, false);
        offBtn.setTooltip("OFF: deactivates channel output. Uses supplied unlit/illuminated button.");
        offBtn.onClick = [this]()
        {
            channelRef.setDisableOutput(true);
            onBtn.setToggled(false, false);
            offBtn.setToggled(true, false);
        };
        addAndMakeVisible(offBtn);

        // ── MUTE button ──────────────────────────────────────────────────
        muteBtn.setToggled(channelRef.getMute(), false);
        muteBtn.setTooltip("MUTE: mutes post-fader output. Independent of ON/OFF.");
        muteBtn.onToggled = [this](bool state)
        {
            channelRef.setMute(state);
        };
        addAndMakeVisible(muteBtn);

        // ── PH button ────────────────────────────────────────────────────
        phaseInvertBtn.setToggled(channelRef.getPhaseInvert(), false);
        phaseInvertBtn.setTooltip("PH: phase / polarity invert (180 deg)");
        phaseInvertBtn.onToggled = [this](bool state)
        {
            channelRef.setPhaseInvert(state);
        };
        addAndMakeVisible(phaseInvertBtn);

        // ── MONO button ──────────────────────────────────────────────────
        monoBtn.setToggled(channelRef.getForceMono(), false);
        monoBtn.setTooltip("MONO: force mono sum (L+R)*0.5");
        monoBtn.onToggled = [this](bool state)
        {
            channelRef.setForceMono(state);
        };
        addAndMakeVisible(monoBtn);

        // ── PFL button ───────────────────────────────────────────────────
        pflBtn.setToggled(channelRef.getPfl(), false);
        pflBtn.setTooltip("PFL: Pre-Fader Listen - routes pre-fader / post-VST signal to monitor output. Mutually exclusive with DM.");
        pflBtn.onToggled = [this](bool state)
        {
            channelRef.setPfl(state);
            if (state)
                directMonitorBtn.setToggled(false, false);
        };
        addAndMakeVisible(pflBtn);

        // ── HPF button ───────────────────────────────────────────────────
        hpfBtn.setButtonEnabled(true);
        hpfBtn.setToggled(channelRef.getHpfEnabled(), false);
        hpfBtn.setTooltip("HPF: 80 Hz 2nd-order Butterworth high-pass filter");
        hpfBtn.onToggled = [this](bool state)
        {
            channelRef.setHpfEnabled(state);
        };
        addAndMakeVisible(hpfBtn);

        // ── PAD button ───────────────────────────────────────────────────
        padBtn.setButtonEnabled(true);
        padBtn.setToggled(channelRef.getPadEnabled(), false);
        padBtn.setTooltip("PAD: -20 dB input attenuation");
        padBtn.onToggled = [this](bool state)
        {
            channelRef.setPadEnabled(state);
        };
        addAndMakeVisible(padBtn);

        // ── VST rack button ──────────────────────────────────────────────
        vstRackBtn.setToggleMode(false);
        vstRackBtn.setTooltip("Open VST3 plugin rack for this channel");
        vstRackBtn.onClick = [this]() { openVstRackWindow(); };
        addAndMakeVisible(vstRackBtn);

        // ── DM button ────────────────────────────────────────────────────
        directMonitorBtn.setToggled(channelRef.getDirectMonitor(), false);
        directMonitorBtn.setTooltip("DM: Direct Monitor - routes post-fader / post-VST signal to monitor output. Mutually exclusive with PFL.");
        directMonitorBtn.onToggled = [this](bool state)
        {
            channelRef.setDirectMonitor(state);
            if (state)
                pflBtn.setToggled(false, false);
        };
        addAndMakeVisible(directMonitorBtn);
    }

    // =========================================================================
    // Open VST rack window
    // =========================================================================
    void ChannelStrip::openVstRackWindow()
    {
        if (activeRackWindow != nullptr)
        {
            activeRackWindow->toFront(true);
            return;
        }
        activeRackWindow = new PluginRackWindow(
            "DSD Mixer - " + channelNameLabel.getText() + " VST3 Rack",
            channelRef);
    }

    // =========================================================================
    // Meter update (called from audio thread timer)
    // =========================================================================
    void ChannelStrip::updateMeterFromAudio()
    {
        const auto& mv = channelRef.getMeterInput();
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float peakMax = juce::jmax(pL, pR);
        const float hL = mv.peakHoldL.load(std::memory_order_relaxed);
        const float hR = mv.peakHoldR.load(std::memory_order_relaxed);
        const bool  clip = mv.clipped.load(std::memory_order_relaxed);

        topMeter.setMeterValues(pL, pR, hL, hR, clip);

        if (peakMax <= 0.001f)
        {
            preGainDbfsReadout.setText("IN: -oo dBFS", juce::dontSendNotification);
            preGainDbfsReadout.setColour(juce::Label::textColourId,
                                         DSDLookAndFeel::getTextSecondary());
        }
        else
        {
            const float db = GainProcessor::linearToDb(peakMax);
            preGainDbfsReadout.setText("IN: " + juce::String(db, 1) + " dBFS",
                                       juce::dontSendNotification);
            if (db >= -0.5f)
                preGainDbfsReadout.setColour(juce::Label::textColourId,
                                             DSDLookAndFeel::getMeterRed());
            else if (db >= -6.0f)
                preGainDbfsReadout.setColour(juce::Label::textColourId,
                                             DSDLookAndFeel::getMeterYellow());
            else
                preGainDbfsReadout.setColour(juce::Label::textColourId,
                                             DSDLookAndFeel::getMeterGreen());
        }

        const int vstCount = channelRef.getPluginRack().getNumPlugins();
        vstRackBtn.setText(vstCount > 0 ? "VST " + juce::String(vstCount) : "VST");
    }

    // =========================================================================
    // Layout — follows exact reference image proportions
    // =========================================================================
    void ChannelStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // ── 1. Input selector at top (20px) ──────────────────────────
        inputDeviceSelector.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        // ── 2. OLED display (82px) ────────────────────────────────────
        auto oledArea = area.removeFromTop(82);
        oledBounds = oledArea.toFloat();

        // Row 1: CH badge (left) + Fader dB readout (right) — 15px
        chNumberBadge.setBounds(oledArea.getX() + 18, oledArea.getY() + 4, 38, 15);
        faderReadout.setBounds(oledArea.getRight() - 48, oledArea.getY() + 4, 44, 15);

        // Row 2: Channel name pill box (centered) — 16px
        const int nameW = std::min(84, oledArea.getWidth() - 16);
        channelNameLabel.setBounds(oledArea.getCentreX() - nameW / 2, oledArea.getY() + 23, nameW, 16);

        // Row 3: IN: dBFS numeric text (centered) — 13px
        preGainDbfsReadout.setBounds(oledArea.getX() + 4, oledArea.getY() + 42, oledArea.getWidth() - 8, 13);

        // Row 4: Horizontal segmented LED meter bar — 21px
        topMeter.setBounds(oledArea.getX() + 6, oledArea.getY() + 57, oledArea.getWidth() - 12, 21);

        area.removeFromTop(5);

        // ── 3. GAIN knob (62px) ──────────────────────────────────────
        gainKnob.setBounds(area.removeFromTop(62));
        area.removeFromTop(4);

        // ── 4. PAN knob (left) + VST/DM buttons (right) — 46px ───────
        {
            auto panRow = area.removeFromTop(46);
            const int halfW = panRow.getWidth() / 2;
            panKnob.setBounds(panRow.removeFromLeft(halfW));

            auto btnSide = panRow;
            vstRackBtn.setBounds(btnSide.removeFromTop(20));
            btnSide.removeFromTop(3);
            directMonitorBtn.setBounds(btnSide.removeFromTop(20));
        }
        area.removeFromTop(4);

        // ── 5. Button matrix row 1: MUTE | PH | MONO (36px) ──────────
        {
            auto row1 = area.removeFromTop(36);
            const int btnW = (row1.getWidth() - 6) / 3;
            muteBtn.setBounds(row1.removeFromLeft(btnW));
            row1.removeFromLeft(3);
            phaseInvertBtn.setBounds(row1.removeFromLeft(btnW));
            row1.removeFromLeft(3);
            monoBtn.setBounds(row1);
        }
        area.removeFromTop(3);

        // ── 6. Button matrix row 2: PFL | HPF | PAD (36px) ───────────
        {
            auto row2 = area.removeFromTop(36);
            const int btnW = 33;
            pflBtn.setBounds(row2.removeFromLeft(btnW));
            row2.removeFromLeft(3);
            hpfBtn.setBounds(row2.removeFromLeft(btnW));
            row2.removeFromLeft(3);
            padBtn.setBounds(row2);
        }
        area.removeFromTop(4);

        // ── 7. ON / OFF buttons at bottom (50px) ──────────────────────
        area.removeFromBottom(2);
        auto botArea = area.removeFromBottom(50);
        const int halfW = (botArea.getWidth() - 6) / 2;
        onBtn.setBounds(botArea.removeFromLeft(halfW));
        botArea.removeFromLeft(6);
        offBtn.setBounds(botArea);

        area.removeFromBottom(4);

        // ── 8. Fader fills remaining space ────────────────────────────
        fader.setBounds(area);
    }

    // =========================================================================
    // Paint — console strip chassis
    // =========================================================================
    void ChannelStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Light grey console chassis body
        g.setColour(DSDLookAndFeel::getConsoleStripBg());
        g.fillRoundedRectangle(bounds, 4.0f);

        // 2. Bevel border around entire strip
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 3. OLED display recessed black window
        if (oledBounds.getWidth() > 0)
        {
            g.setColour(DSDLookAndFeel::getOledBlack());
            g.fillRoundedRectangle(oledBounds, 3.5f);
            g.setColour(juce::Colour(0xff2A2E38));
            g.drawRoundedRectangle(oledBounds, 3.5f, 1.0f);

            // Red live indicator dot on left of CH badge
            const float dotX = oledBounds.getX() + 8.0f;
            const float dotY = oledBounds.getY() + 11.5f;

            // Outer red halo
            g.setColour(juce::Colour(0x35FF2020));
            g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);

            // Dark red bezel rim
            g.setColour(juce::Colour(0xff881010));
            g.fillEllipse(dotX - 3.8f, dotY - 3.8f, 7.6f, 7.6f);

            // Bright illuminated core
            g.setColour(juce::Colour(0xffFF2828));
            g.fillEllipse(dotX - 2.8f, dotY - 2.8f, 5.6f, 5.6f);

            // Specular lens highlight
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.fillEllipse(dotX - 1.2f, dotY - 1.6f, 1.8f, 1.8f);
        }
    }

} // namespace dsd
