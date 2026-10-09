#include "UI/OutputBayStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "Audio/MultiDeviceManager.h"
#include <cmath>

namespace dsd
{
    OutputBayStrip::OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager)
        : outputBusRef(bus), devMgrRef(deviceManager), stereoMeter(true, false)
    {
        // 1. Output Device selector dropdown (aligns with ChannelStrip input selector)
        outputDeviceSelector.setTextWhenNothingSelected("None");
        outputDeviceSelector.setJustificationType(juce::Justification::centred);
        outputDeviceSelector.onBeforePopup = [this]() { refreshDeviceList(); };
        outputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(outputDeviceSelector);
        refreshDeviceList();

        // 2. Bus Identifier Badge ("OUT 1", "OUT 2", etc.)
        busIdBadge.setText(juce::String::formatted("OUT %d", outputBusRef.getBusID()), juce::dontSendNotification);
        busIdBadge.setJustificationType(juce::Justification::centred);
        busIdBadge.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        busIdBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff221A0C));
        busIdBadge.setColour(juce::Label::outlineColourId,    juce::Colour(0xffB45309));
        busIdBadge.setColour(juce::Label::textColourId,       juce::Colour(0xffFDE68A));
        busIdBadge.setTooltip("Master output bus identifier");
        addAndMakeVisible(busIdBadge);

        // 3. Fader dB readout (top-right of OLED)
        faderReadout.setJustificationType(juce::Justification::centredRight);
        faderReadout.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        faderReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        faderReadout.setTooltip("Output fader level (dB)");
        addAndMakeVisible(faderReadout);

        // 4. Bus Name Label (Clickable & Editable)
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);
        busNameLabel.setJustificationType(juce::Justification::centred);
        busNameLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        busNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff14171E));
        busNameLabel.setColour(juce::Label::outlineColourId,    juce::Colour(0xff2B303C));
        busNameLabel.setColour(juce::Label::textColourId,       DSDLookAndFeel::getTextOnOled());
        busNameLabel.setEditable(true);
        busNameLabel.setTooltip("Click to rename output bus");
        busNameLabel.onTextChange = [this]()
        {
            outputBusRef.setName(busNameLabel.getText().toStdString());
        };
        addAndMakeVisible(busNameLabel);

        // 5. Bus Role / Status Label ("MASTER BUS" or "MONITOR DEST")
        busStatusLabel.setJustificationType(juce::Justification::centred);
        busStatusLabel.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        addAndMakeVisible(busStatusLabel);
        updateBusStatusLabel();

        // 6. Dynamic Peak dBFS Readout ("OUT: -oo dBFS")
        outPeakDbfsLabel.setText("OUT: -oo dBFS", juce::dontSendNotification);
        outPeakDbfsLabel.setJustificationType(juce::Justification::centred);
        outPeakDbfsLabel.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        outPeakDbfsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8C929D));
        outPeakDbfsLabel.setTooltip("Master output peak level (dBFS)");
        addAndMakeVisible(outPeakDbfsLabel);

        // 7. Stereo Level Meter (L / R)
        stereoMeter.setClipResetCallback([this]()
        {
            outputBusRef.getMeterValues().resetClip();
        });
        stereoMeter.setTooltip("Master stereo output meter (L/R peak and peak-hold)");
        addAndMakeVisible(stereoMeter);

        // 8. Broadcast Utility Buttons: MUTE and MON (using supplied button assets)
        setupButtons();

        // 9. Console Fader
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            outputBusRef.setFaderDb(db);
            updateGainReadout(db);
        });
        addAndMakeVisible(fader);

        // 10. Gain dB Numeric Readout pill at bottom
        gainReadout.setJustificationType(juce::Justification::centred);
        gainReadout.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        gainReadout.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        gainReadout.setColour(juce::Label::outlineColourId,    juce::Colours::transparentBlack);
        gainReadout.setColour(juce::Label::textColourId,       DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(gainReadout);
        updateGainReadout(outputBusRef.getFaderDb());
    }

    void OutputBayStrip::updateBusStatusLabel()
    {
        if (outputBusRef.getMonitor())
        {
            busStatusLabel.setText("MONITOR DEST", juce::dontSendNotification);
            busStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
            busStatusLabel.setTooltip("Active monitor listen destination");
        }
        else
        {
            busStatusLabel.setText("MASTER BUS", juce::dontSendNotification);
            busStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8C929D));
            busStatusLabel.setTooltip("Standard master mix bus");
        }
    }

    void OutputBayStrip::updateGainReadout(float db)
    {
        juce::String text;
        if (db <= -59.5f)
            text = "-inf dB";
        else
            text = (db > 0.0f ? "+" : "") + juce::String(db, 1) + " dB";

        gainReadout.setText(text, juce::dontSendNotification);
        faderReadout.setText(text, juce::dontSendNotification);
    }

    void OutputBayStrip::refreshDeviceList()
    {
        outputDeviceSelector.clear(juce::dontSendNotification);

        // 1. None option
        outputDeviceSelector.addItem("None", 1);

        // 2. Direct Primary Hardware Master Output
        outputDeviceSelector.addSectionHeading("[ Master Hardware Output ]");
        outputDeviceSelector.addItem("Default Master Out (Ch 1-2) [Clean Direct]", 10);

        // 3. Real Windows Audio Output Devices (WASAPI)
        auto winOutputs = MultiDeviceManager::getInstance().getAvailableOutputDevices();
        if (!winOutputs.isEmpty())
        {
            outputDeviceSelector.addSectionHeading("[ Windows Audio Devices ]");
            for (int i = 0; i < winOutputs.size(); ++i)
            {
                outputDeviceSelector.addItem(winOutputs[i], 100 + i);
            }
        }

        // Match current assigned device name
        const juce::String currentDev = outputBusRef.getOutputDeviceName();
        const int chOffset = outputBusRef.getDeviceChannelOffset();

        if (currentDev.isEmpty() || currentDev == "None")
        {
            if (chOffset >= 0)
                outputDeviceSelector.setSelectedId(10, juce::dontSendNotification);
            else
                outputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
        }
        else if (currentDev.containsIgnoreCase("Default Master") || currentDev.containsIgnoreCase("Master Out"))
        {
            outputDeviceSelector.setSelectedId(10, juce::dontSendNotification);
        }
        else
        {
            int foundId = -1;
            for (int i = 0; i < winOutputs.size(); ++i)
            {
                if (winOutputs[i] == currentDev)
                {
                    foundId = 100 + i;
                    break;
                }
            }
            if (foundId > 0)
                outputDeviceSelector.setSelectedId(foundId, juce::dontSendNotification);
            else
                outputDeviceSelector.setText(currentDev, juce::dontSendNotification);
        }
    }

    void OutputBayStrip::updateUIFromBus()
    {
        busIdBadge.setText(juce::String::formatted("OUT %d", outputBusRef.getBusID()), juce::dontSendNotification);
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);
        updateBusStatusLabel();
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        updateGainReadout(outputBusRef.getFaderDb());
        muteBtn.setToggled(outputBusRef.getMute(), false);
        monitorBtn.setToggled(outputBusRef.getMonitor(), false);
    }

    void OutputBayStrip::onDeviceSelected()
    {
        const int id = outputDeviceSelector.getSelectedId();

        if (id <= 1)
        {
            outputBusRef.setDeviceChannelOffset(-1);
            outputBusRef.setOutputDeviceName("None");
        }
        else if (id == 10)
        {
            // Direct master DAC output callback (zero latency, zero buffer conflict)
            outputBusRef.setDeviceChannelOffset(0);
            outputBusRef.setOutputDeviceName("Default Master Out");
        }
        else if (id >= 100)
        {
            const juce::String selectedName = outputDeviceSelector.getText();
            const juce::String primaryName = MultiDeviceManager::getInstance().getPrimaryOutputDeviceName();

            if (primaryName.isNotEmpty() && selectedName.equalsIgnoreCase(primaryName))
            {
                // Matches host primary output device: render directly to master DAC channels!
                outputBusRef.setDeviceChannelOffset(0);
                outputBusRef.setOutputDeviceName(selectedName.toStdString());
            }
            else
            {
                // Dedicated secondary output device: render via dedicated WindowsDeviceOutputSink only!
                outputBusRef.setDeviceChannelOffset(-1);
                outputBusRef.setOutputDeviceName(selectedName.toStdString());
            }
        }
    }

    void OutputBayStrip::setupButtons()
    {
        // MUTE button: uses supplied OnButton.png / OffButton.png
        muteBtn.setToggleMode(true);
        muteBtn.setToggled(outputBusRef.getMute(), false);
        muteBtn.setTooltip("MUTE: Mutes output bus audio");
        muteBtn.onToggled = [this](bool state)
        {
            outputBusRef.setMute(state);
        };
        addAndMakeVisible(muteBtn);

        // MON button: uses supplied OnButton.png / OffButton.png
        monitorBtn.setToggleMode(true);
        monitorBtn.setToggled(outputBusRef.getMonitor(), false);
        monitorBtn.setTooltip("MON: Monitor Listen Destination (Receives Direct Monitor / PFL from input channels)");
        monitorBtn.onToggled = [this](bool state)
        {
            outputBusRef.setMonitor(state);
            updateBusStatusLabel();
        };
        addAndMakeVisible(monitorBtn);
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

        const float peakMax = juce::jmax(pL, pR);
        if (peakMax <= 0.001f)
        {
            outPeakDbfsLabel.setText("OUT: -oo dBFS", juce::dontSendNotification);
            outPeakDbfsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8C929D));
        }
        else
        {
            const float db = 20.0f * std::log10(std::max(peakMax, 0.000001f));
            outPeakDbfsLabel.setText("OUT: " + juce::String(db, 1) + " dBFS", juce::dontSendNotification);
            if (db >= -0.5f)
                outPeakDbfsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterRed());
            else if (db >= -6.0f)
                outPeakDbfsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
            else
                outPeakDbfsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterGreen());
        }
    }

    void OutputBayStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Output selector at top (20px) — aligns with ChannelStrip's input selector
        outputDeviceSelector.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        // 2. Recessed OLED display card (78px) — aligns with ChannelStrip's OLED area
        auto oledArea = area.removeFromTop(78);
        oledBounds = oledArea.toFloat();

        // Row 1: OUT badge (left) + Fader dB readout (right) — 15px
        busIdBadge.setBounds(oledArea.getX() + 15, oledArea.getY() + 3, 34, 15);
        faderReadout.setBounds(oledArea.getRight() - 38, oledArea.getY() + 3, 36, 15);

        // Row 2: Bus Name pill box (centered) — 17px
        const int nameW = std::min(86, oledArea.getWidth() - 10);
        busNameLabel.setBounds(oledArea.getCentreX() - nameW / 2, oledArea.getY() + 22, nameW, 17);

        // Row 3: Status / role label (centered) — 14px
        busStatusLabel.setBounds(oledArea.getX() + 4, oledArea.getY() + 42, oledArea.getWidth() - 8, 14);

        // Row 4: Peak dBFS readout (centered) — 14px
        outPeakDbfsLabel.setBounds(oledArea.getX() + 4, oledArea.getY() + 58, oledArea.getWidth() - 8, 14);

        area.removeFromTop(6);

        // 3. Stereo Meter (106px tall, 38px wide, centered)
        const int meterW = 38;
        const int meterH = 106;
        stereoMeter.setBounds(area.removeFromTop(meterH).withSizeKeepingCentre(meterW, meterH));

        area.removeFromTop(8);

        // 4. Utility buttons: MUTE and MON side-by-side (38px tall, 34px wide each)
        {
            auto btnRow = area.removeFromTop(38);
            const int btnW = 34;
            const int btnH = 38;
            const int btnGap = 8;
            const int totalW = btnW * 2 + btnGap;
            const int startX = btnRow.getCentreX() - totalW / 2;
            muteBtn.setBounds(startX, btnRow.getY(), btnW, btnH);
            monitorBtn.setBounds(startX + btnW + btnGap, btnRow.getY(), btnW, btnH);
        }

        area.removeFromTop(6);

        // 5. Gain Readout pill at bottom (20px tall, 66px wide, centered)
        const int pillW = 66;
        const int pillH = 20;
        gainReadoutBounds = area.removeFromBottom(pillH).withSizeKeepingCentre(pillW, pillH).toFloat();
        gainReadout.setBounds(gainReadoutBounds.toNearestInt());

        area.removeFromBottom(6);

        // 6. Output Fader fills all remaining vertical space
        fader.setBounds(area);
    }

    void OutputBayStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Console chassis light grey
        g.setColour(juce::Colour(0xffC6C9CF));
        g.fillRoundedRectangle(bounds, 4.0f);

        // 2. Bevel border around strip
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // 3. OLED recessed black window
        if (oledBounds.getWidth() > 0)
        {
            g.setColour(DSDLookAndFeel::getOledBlack());
            g.fillRoundedRectangle(oledBounds, 3.5f);
            g.setColour(juce::Colour(0xff2A2E38));
            g.drawRoundedRectangle(oledBounds, 3.5f, 1.0f);

            // Amber live indicator dot on left of OUT badge
            const float dotX = oledBounds.getX() + 8.0f;
            const float dotY = oledBounds.getY() + 11.5f;

            // Outer amber halo
            g.setColour(juce::Colour(0x35FFA000));
            g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);

            // Dark bronze bezel rim
            g.setColour(juce::Colour(0xff663800));
            g.fillEllipse(dotX - 3.8f, dotY - 3.8f, 7.6f, 7.6f);

            // Bright amber core
            g.setColour(juce::Colour(0xffFFA000));
            g.fillEllipse(dotX - 2.8f, dotY - 2.8f, 5.6f, 5.6f);

            // Specular lens highlight
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.fillEllipse(dotX - 1.2f, dotY - 1.6f, 1.8f, 1.8f);
        }

        // 4. Recessed OLED pill behind bottom gain readout
        if (gainReadoutBounds.getWidth() > 0)
        {
            g.setColour(DSDLookAndFeel::getOledBlack());
            g.fillRoundedRectangle(gainReadoutBounds, 3.0f);
            g.setColour(juce::Colour(0xff2A2E38));
            g.drawRoundedRectangle(gainReadoutBounds, 3.0f, 1.0f);
        }
    }
} // namespace dsd
