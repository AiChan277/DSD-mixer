#include "UI/MasterStrip.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    MasterStrip::MasterStrip(MasterBus& masterBus)
        : masterRef(masterBus),
          stereoMeter(true) // Stereo dual L/R meter
    {
        // 1. Device Out Name display
        deviceOutNameLabel.setText(masterRef.getDeviceOutName(), juce::dontSendNotification);
        deviceOutNameLabel.setJustificationType(juce::Justification::centred);
        deviceOutNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        deviceOutNameLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        deviceOutNameLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
        deviceOutNameLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(deviceOutNameLabel);

        // 2. Setup Buttons
        setupButtons();

        // 3. Stereo Meter with clip reset
        stereoMeter.setClipResetCallback([this]()
        {
            masterRef.getMeterValues().resetClip();
        });
        addAndMakeVisible(stereoMeter);

        // 4. Stereo Master Fader
        masterFader.setValue(masterRef.getFaderDb(), juce::dontSendNotification);
        masterFader.setOnValueChanged([this](float db)
        {
            masterRef.setFaderDb(db);
        });
        addAndMakeVisible(masterFader);

        // 5. Bottom Master Label
        masterBottomLabel.setText("MASTER", juce::dontSendNotification);
        masterBottomLabel.setJustificationType(juce::Justification::centred);
        masterBottomLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        masterBottomLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentRed().withAlpha(0.35f));
        masterBottomLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
        masterBottomLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(masterBottomLabel);
    }

    void MasterStrip::setupButtons()
    {
        // Mute Button (Red)
        muteBtn.setClickingTogglesState(true);
        muteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff450a0a));
        muteBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentRed());
        muteBtn.setTooltip("Mute Master Output");
        muteBtn.onClick = [this]()
        {
            masterRef.setMute(muteBtn.getToggleState());
        };
        addAndMakeVisible(muteBtn);

        // Monitor Button (Amber/Green)
        monitorBtn.setClickingTogglesState(true);
        monitorBtn.setToggleState(true, juce::dontSendNotification);
        monitorBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14532d));
        monitorBtn.setColour(juce::TextButton::buttonOnColourId, DSDLookAndFeel::getAccentGreen());
        monitorBtn.setTooltip("Toggle Studio Monitor Output");
        monitorBtn.onClick = [this]()
        {
            masterRef.setMonitor(monitorBtn.getToggleState());
        };
        addAndMakeVisible(monitorBtn);
    }

    void MasterStrip::updateMeterFromAudio()
    {
        const auto& mv = masterRef.getMeterValues();
        const float pL = mv.peakL.load(std::memory_order_relaxed);
        const float pR = mv.peakR.load(std::memory_order_relaxed);
        const float hL = mv.peakHoldL.load(std::memory_order_relaxed);
        const float hR = mv.peakHoldR.load(std::memory_order_relaxed);
        const bool clip = mv.clipped.load(std::memory_order_relaxed);

        stereoMeter.setMeterValues(pL, pR, hL, hR, clip);
    }

    void MasterStrip::resized()
    {
        auto area = getLocalBounds().reduced(4, 4);

        // 1. Top: Device Out Name display
        deviceOutNameLabel.setBounds(area.removeFromTop(24));

        area.removeFromTop(6);

        // 2. Buttons: MUTE & MONITOR side-by-side
        auto btnRow = area.removeFromTop(28);
        const int btnW = (btnRow.getWidth() - 4) / 2;
        muteBtn.setBounds(btnRow.removeFromLeft(btnW));
        btnRow.removeFromLeft(4);
        monitorBtn.setBounds(btnRow);

        area.removeFromTop(8);

        // 3. Dual L/R Level Meters
        stereoMeter.setBounds(area.removeFromTop(80));

        area.removeFromTop(8);

        // 4. Bottom: Master Label
        masterBottomLabel.setBounds(area.removeFromBottom(28));

        area.removeFromBottom(6);

        // 5. Stereo Master Fader
        masterFader.setBounds(area);
    }

    void MasterStrip::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Distinct Master Bay background (slightly darker brushed finish with red accent)
        g.setColour(juce::Colour(0xff292b30));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Accent border for master bay
        g.setColour(juce::Colour(0xff575c68));
        g.drawRoundedRectangle(bounds, 4.0f, 1.4f);
    }
} // namespace dsd
