#include "UI/OutputBayStrip.h"
#include "UI/DSDLookAndFeel.h"
#include "Audio/MultiDeviceManager.h"

namespace dsd
{
    OutputBayStrip::OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager)
        : outputBusRef(bus), devMgrRef(deviceManager), stereoMeter(true)
    {
        // 1. Output Device selector at top
        outputDeviceSelector.setTextWhenNothingSelected("None");
        outputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        addAndMakeVisible(outputDeviceSelector);
        refreshDeviceList();

        // 2. Bus Name Label (Clickable & Editable)
        busNameLabel.setText(outputBusRef.getName(), juce::dontSendNotification);
        busNameLabel.setJustificationType(juce::Justification::centred);
        busNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        busNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        busNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
        busNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        busNameLabel.setEditable(true);
        busNameLabel.onTextChange = [this]()
        {
            outputBusRef.setName(busNameLabel.getText().toStdString());
        };
        addAndMakeVisible(busNameLabel);

        // 3. Illuminated Amber Buttons setup
        setupButtons();

        // 4. Stereo Meter
        stereoMeter.setClipResetCallback([this]()
        {
            outputBusRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(stereoMeter);

        // 5. Output Fader
        fader.setValue(outputBusRef.getFaderDb(), juce::dontSendNotification);
        fader.setOnValueChanged([this](float db)
        {
            outputBusRef.setFaderDb(db);
        });
        addAndMakeVisible(fader);
    }

    void OutputBayStrip::refreshDeviceList()
    {
        outputDeviceSelector.clear(juce::dontSendNotification);

        // 1. None option
        outputDeviceSelector.addItem("None", 1);

        // 2. Real Windows Audio Output Devices
        auto winOutputs = MultiDeviceManager::getInstance().getAvailableOutputDevices();
        if (!winOutputs.isEmpty())
        {
            outputDeviceSelector.addSectionHeading("── Windows Outputs ──");
            for (int i = 0; i < winOutputs.size(); ++i)
            {
                outputDeviceSelector.addItem(winOutputs[i], 100 + i);
            }
        }

        // Match current assigned device name
        const juce::String currentDev = outputBusRef.getOutputDeviceName();
        if (currentDev.isEmpty() || currentDev == "None")
        {
            outputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
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

    void OutputBayStrip::onDeviceSelected()
    {
        const int id = outputDeviceSelector.getSelectedId();

        if (id <= 1)
        {
            outputBusRef.setDeviceChannelOffset(-1);
            outputBusRef.setOutputDeviceName("None");
        }
        else if (id >= 100)
        {
            const juce::String selectedName = outputDeviceSelector.getText();
            outputBusRef.setOutputDeviceName(selectedName.toStdString());
            outputBusRef.setDeviceChannelOffset(0);
        }
    }

    void OutputBayStrip::setupButtons()
    {
        // Mute Button - Illuminated Amber when ON
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

        // 1. Output Device selector at top
        outputDeviceSelector.setBounds(area.removeFromTop(22));

        area.removeFromTop(4);

        // 2. Bus Name Label
        busNameLabel.setBounds(area.removeFromTop(24));

        area.removeFromTop(5);

        // 3. MUTE and MON side by side
        auto btnRow = area.removeFromTop(26);
        const int halfW = (btnRow.getWidth() - 4) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(halfW));
        btnRow.removeFromLeft(4);
        monitorBtn.setBounds(btnRow);

        area.removeFromTop(6);

        // 4. Stereo Meter
        stereoMeter.setBounds(area.removeFromTop(120));

        area.removeFromTop(6);

        // 5. Output Fader fills remainder
        fader.setBounds(area);
    }

    void OutputBayStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xffC6C9CF));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
