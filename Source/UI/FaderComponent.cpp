#include "UI/FaderComponent.h"
#include "UI/DSDLookAndFeel.h"
#include "Audio/AudioTypes.h"

namespace dsd
{
    FaderComponent::FaderComponent(const juce::String& name)
    {
        slider.setName(name);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        // Range -60 dB to +12 dB, default 0 dB
        slider.setRange(MIN_FADER_DB, MAX_FADER_DB, 0.1);
        slider.setValue(0.0, juce::dontSendNotification);
        slider.setSkewFactorFromMidPoint(-6.0); // Natural console fader curve around 0dB
        slider.setDoubleClickReturnValue(true, 0.0);

        slider.onValueChange = [this]()
        {
            const float val = static_cast<float>(slider.getValue());
            if (val <= -59.5f)
                valueLabel.setText("-inf dB", juce::dontSendNotification);
            else
                valueLabel.setText(juce::String(val, 1) + " dB", juce::dontSendNotification);

            if (onValueChanged)
                onValueChanged(val);
        };

        addAndMakeVisible(slider);

        valueLabel.setText("0.0 dB", juce::dontSendNotification);
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        valueLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        valueLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        addAndMakeVisible(valueLabel);
    }

    void FaderComponent::setValue(float dbValue, juce::NotificationType notification)
    {
        slider.setValue(dbValue, notification);
    }

    float FaderComponent::getValue() const noexcept
    {
        return static_cast<float>(slider.getValue());
    }

    void FaderComponent::resized()
    {
        auto bounds = getLocalBounds();
        valueLabel.setBounds(bounds.removeFromTop(18).reduced(4, 0));
        slider.setBounds(bounds);
    }

    void FaderComponent::paint(juce::Graphics& g)
    {
        const auto sliderBounds = slider.getBounds().toFloat();
        const float trackCenterX = sliderBounds.getX() + sliderBounds.getWidth() * 0.40f;
        const float trackTop = sliderBounds.getY() + 10.0f;
        const float trackHeight = sliderBounds.getHeight() - 20.0f;

        const struct { float db; const char* label; } markings[] = {
            { 12.0f, "+12" },
            {  6.0f,  "+6" },
            {  0.0f,   "0" },
            { -6.0f,  "-6" },
            {-12.0f, "-12" },
            {-24.0f, "-24" },
            {-36.0f, "-36" },
            {-48.0f, "-48" },
            {-60.0f, "-oo" }
        };

        for (const auto& m : markings)
        {
            const float prop = static_cast<float>(slider.valueToProportionOfLength(m.db));
            const float y = trackTop + trackHeight * (1.0f - prop);
            const bool isUnity = (std::abs(m.db) < 0.01f);

            // Left tick mark
            g.setColour(isUnity ? DSDLookAndFeel::getAccentAmber() : DSDLookAndFeel::getTextPrimary().withAlpha(0.6f));
            g.fillRect(trackCenterX - 18.0f, y - 0.5f, 3.5f, isUnity ? 1.5f : 1.0f);

            // Right tick mark
            const float rightTickLen = isUnity ? 6.0f : 3.5f;
            g.fillRect(trackCenterX + 15.0f, y - 0.5f, rightTickLen, isUnity ? 1.5f : 1.0f);

            // Text label
            if (isUnity)
            {
                g.setColour(DSDLookAndFeel::getAccentAmber());
                g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            }
            else
            {
                g.setColour(DSDLookAndFeel::getTextPrimary().withAlpha(0.75f));
                g.setFont(juce::FontOptions(8.5f));
            }

            g.drawText(m.label,
                       static_cast<int>(trackCenterX + 22.0f),
                       static_cast<int>(y - 6.0f),
                       24, 12,
                       juce::Justification::centredLeft, false);
        }
    }
} // namespace dsd
