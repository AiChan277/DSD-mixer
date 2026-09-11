#include "UI/OutputBayStrip.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    OutputBayStrip::OutputBayStrip(OutputBus& bus, juce::AudioDeviceManager& deviceManager)
        : outputBusRef(bus), devMgrRef(deviceManager), stereoMeter(true)
    {
        addAndMakeVisible(outputDeviceSelector);
        outputDeviceSelector.onChange = [this]() { onDeviceSelected(); };
        refreshDeviceList();

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

        setupButtons();

        stereoMeter.setClipResetCallback([this]()
        {
            outputBusRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(stereoMeter);

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
        outputDeviceSelector.addItem("None", 1);
        
        if (auto* device = devMgrRef.getCurrentAudioDevice())
        {
            auto outputNames = device->getOutputChannelNames();
            for (int i = 0; i < outputNames.size(); i += 2)
            {
                int id = i / 2 + 2;
                juce::String label;
                if (i + 1 < outputNames.size())
                    label = outputNames[i] + " / " + outputNames[i + 1];
                else
                    label = outputNames[i];
                outputDeviceSelector.addItem(label, id);
            }
        }

        int currentId = 1;
        int currentOffset = outputBusRef.getDeviceChannelOffset();
        if (currentOffset >= 0)
            currentId = (currentOffset / 2) + 2;
        outputDeviceSelector.setSelectedId(currentId, juce::dontSendNotification);
    }

    void OutputBayStrip::onDeviceSelected()
    {
        int selected = outputDeviceSelector.getSelectedId();
        if (selected <= 1)
        {
            outputBusRef.setDeviceChannelOffset(-1);
            outputBusRef.setOutputDeviceName("None");
        }
        else
        {
            int offset = (selected - 2) * 2;
            outputBusRef.setDeviceChannelOffset(offset);
            outputBusRef.setOutputDeviceName(outputDeviceSelector.getText().toStdString());
        }
    }

    void OutputBayStrip::setupButtons()
    {
        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8B2020));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        muteBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        muteBtn.onClick = [this]() { outputBusRef.setMute(muteBtn.getToggleState()); };
        addAndMakeVisible(muteBtn);

        monitorBtn.setClickingTogglesState(true);
        monitorBtn.setToggleState(outputBusRef.getMonitor(), juce::dontSendNotification);
        monitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff064e3b));
        monitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        monitorBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        monitorBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF));
        monitorBtn.onClick = [this]() { outputBusRef.setMonitor(monitorBtn.getToggleState()); };
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
        outputDeviceSelector.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        
        busNameLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        
        auto btnRow = area.removeFromTop(26);
        muteBtn.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2 - 2));
        btnRow.removeFromLeft(4);
        monitorBtn.setBounds(btnRow);
        area.removeFromTop(8);
        
        stereoMeter.setBounds(area.removeFromTop(120));
        area.removeFromTop(8);
        
        fader.setBounds(area);
    }

    void OutputBayStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xffC8CACF));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
}
