#include "UI/OutputBayStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "Audio/MultiDeviceManager.h"

namespace dsd
{
    OutputBayStrip::OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager)
        : outputBusRef(bus), devMgrRef(deviceManager), stereoMeter(true)
    {
        // 1. Bus Identifier Label ("OUT 1", "OUT 2", etc.)
        busIdLabel.setText(juce::String::formatted("OUT %d", outputBusRef.getBusID()), juce::dontSendNotification);
        busIdLabel.setJustificationType(juce::Justification::centred);
        busIdLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        busIdLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(busIdLabel);

        // 2. Bus Name Label (Clickable & Editable)
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);
        busNameLabel.setJustificationType(juce::Justification::centred);
        busNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        busNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        busNameLabel.setEditable(true);
        busNameLabel.setTooltip("Click to rename output bus");
        busNameLabel.onTextChange = [this]()
        {
            outputBusRef.setName(busNameLabel.getText().toStdString());
        };
        addAndMakeVisible(busNameLabel);

        // 3. Output Device selector dropdown
        outputDeviceSelector.setTextWhenNothingSelected("None");
        outputDeviceSelector.setJustificationType(juce::Justification::centred);
        outputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(outputDeviceSelector);
        refreshDeviceList();

        // 4. Broadcast Utility Buttons: MUTE and MON
        setupButtons();

        // 5. Stereo Level Meter (L / R)
        stereoMeter.setClipResetCallback([this]()
        {
            outputBusRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(stereoMeter);

        // 6. Console Fader
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            outputBusRef.setFaderDb(db);
            updateGainReadout(db);
        });
        addAndMakeVisible(fader);

        // 7. Gain dB Numeric Readout at bottom
        gainReadout.setJustificationType(juce::Justification::centred);
        gainReadout.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        gainReadout.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        gainReadout.setColour(juce::Label::outlineColourId, juce::Colour(0xff2F3542));
        gainReadout.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(gainReadout);
        updateGainReadout(outputBusRef.getFaderDb());
    }

    void OutputBayStrip::updateGainReadout(float db)
    {
        if (db <= -59.5f)
            gainReadout.setText("-inf dB", juce::dontSendNotification);
        else
            gainReadout.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
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
        // 1. Output Device Selector
        refreshDeviceList();

        // 2. Bus Identifier and Name
        busIdLabel.setText(juce::String::formatted("OUT %d", outputBusRef.getBusID()), juce::dontSendNotification);
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);

        // 3. Fader & Gain Readout
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        updateGainReadout(outputBusRef.getFaderDb());

        // 4. Buttons
        muteBtn.setToggleState(outputBusRef.getMute(), juce::dontSendNotification);
        monitorBtn.setToggleState(outputBusRef.getMonitor(), juce::dontSendNotification);
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
                // Offset MUST be -1 so OutputManager does NOT duplicate it into primary device!
                outputBusRef.setDeviceChannelOffset(-1);
                outputBusRef.setOutputDeviceName(selectedName.toStdString());
            }
        }
    }

    void OutputBayStrip::setupButtons()
    {
        // Mute Button - Illuminated Red alert when ON
        muteBtn.setClickingTogglesState(true);
        muteBtn.setTooltip("Mute Output Bus");
        muteBtn.onClick = [this]()
        {
            outputBusRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Monitor Button - Illuminated Amber when ON
        monitorBtn.setClickingTogglesState(true);
        monitorBtn.setToggleState(outputBusRef.getMonitor(), juce::dontSendNotification);
        monitorBtn.setTooltip("Toggle Monitor Listen");
        monitorBtn.onClick = [this]()
        {
            outputBusRef.setMonitor(monitorBtn.getToggleState());
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
    }

    void OutputBayStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Bus Header: OUT 1 / STUDIO MONITOR (height: 44px)
        auto headerArea = area.removeFromTop(44);
        busIdLabel.setBounds(headerArea.removeFromTop(18));
        busNameLabel.setBounds(headerArea);

        area.removeFromTop(6);

        // 2. Output Device dropdown: [ Default ▼ ] (height: 24px)
        outputDeviceSelector.setBounds(area.removeFromTop(24));

        area.removeFromTop(6);

        // 3. Utility buttons: MUTE and MON side-by-side (height: 26px)
        auto btnRow = area.removeFromTop(26);
        const int halfW = (btnRow.getWidth() - 4) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(halfW));
        btnRow.removeFromLeft(4);
        monitorBtn.setBounds(btnRow);

        area.removeFromTop(6);

        // 4. Dual Stereo Meter: L and R bars (height: 110px, centered)
        const int meterW = std::min(48, area.getWidth() - 8);
        stereoMeter.setBounds(area.removeFromTop(110).withSizeKeepingCentre(meterW, 110));

        area.removeFromTop(6);

        // 5. Gain Readout at bottom: 0.0 dB (height: 22px, centered)
        const int readoutW = std::min(76, area.getWidth());
        gainReadout.setBounds(area.removeFromBottom(22).withSizeKeepingCentre(readoutW, 22));

        area.removeFromBottom(4); // Cushion between fader bottom and gain readout

        // 6. Output Fader fills all remaining vertical space
        fader.setBounds(area);
    }

    void OutputBayStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Console chassis light grey
        g.setColour(juce::Colour(0xffC6C9CF));
        g.fillRoundedRectangle(bounds, 4.0f);

        // 2. Recessed OLED display card behind OUT 1 and STUDIO MONITOR
        auto headerBounds = juce::Rectangle<float>(bounds.getX() + 4.0f,
                                                   bounds.getY() + 4.0f,
                                                   bounds.getWidth() - 8.0f,
                                                   44.0f);
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(headerBounds, 3.0f);
        g.setColour(juce::Colour(0xff2F3542));
        g.drawRoundedRectangle(headerBounds, 3.0f, 1.0f);

        // 3. Strip chassis bevel
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
