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
            if (onValueChanged)
                onValueChanged(static_cast<float>(slider.getValue()));
        };

        addAndMakeVisible(slider);
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
        slider.setBounds(getLocalBounds().reduced(0, 16));
    }

    void FaderComponent::paint(juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const float trackCenterX = std::floor(bounds.getX() + bounds.getWidth() * 0.50f);

        const float topDbPos = static_cast<float>(slider.getY() + slider.getPositionOfValue(MAX_FADER_DB));
        const float botDbPos = static_cast<float>(slider.getY() + slider.getPositionOfValue(MIN_FADER_DB));
        const float trackTop = std::min(topDbPos, botDbPos);
        const float trackBottom = std::max(topDbPos, botDbPos);

        // 1. Allen hex-socket screws at top and bottom of fader track
        const float screwRadius = 4.5f;
        const float topScrewY = trackTop - 10.0f;
        const float botScrewY = trackBottom + 10.0f;

        auto drawAllenScrew = [&](float cx, float cy)
        {
            // Drop shadow
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillEllipse(cx - screwRadius, cy - screwRadius + 1.0f, screwRadius * 2.0f, screwRadius * 2.0f);

            // Metallic bolt head
            juce::ColourGradient boltGrad(juce::Colour(0xffC8CCD4), cx - 2.0f, cy - 2.0f,
                                          juce::Colour(0xff70747E), cx + 2.0f, cy + 2.0f, true);
            g.setGradientFill(boltGrad);
            g.fillEllipse(cx - screwRadius, cy - screwRadius, screwRadius * 2.0f, screwRadius * 2.0f);

            // Circular rim
            g.setColour(juce::Colour(0xff4A4D55));
            g.drawEllipse(cx - screwRadius, cy - screwRadius, screwRadius * 2.0f, screwRadius * 2.0f, 0.8f);

            // 6-sided hexagon Allen socket
            const float hexR = 2.0f;
            juce::Path hex;
            for (int i = 0; i < 6; ++i)
            {
                const float angle = i * juce::MathConstants<float>::twoPi / 6.0f;
                const float hx = cx + hexR * std::cos(angle);
                const float hy = cy + hexR * std::sin(angle);
                if (i == 0) hex.startNewSubPath(hx, hy);
                else hex.lineTo(hx, hy);
            }
            hex.closeSubPath();
            g.setColour(juce::Colour(0xff1E2024));
            g.fillPath(hex);
        };

        if (topScrewY >= 4.0f)
            drawAllenScrew(trackCenterX, topScrewY);
        if (botScrewY <= bounds.getBottom() - 4.0f)
            drawAllenScrew(trackCenterX, botScrewY);

        // 2. Dual-sided precision tick mark scale (DHD Broadcast Console style)
        const struct { float db; const char* label; } markings[] = {
            { 10.0f, "+10" },
            {  5.0f,  "+5" },
            {  0.0f,   "0" },
            { -5.0f,  "-5" },
            {-10.0f, "-10" },
            {-15.0f, "-15" },
            {-20.0f, "-20" },
            {-30.0f, "-30" },
            {-40.0f, "-40" },
            {-50.0f, "-50" },
            {-60.0f, "-oo" }
        };

        for (const auto& m : markings)
        {
            const float y = static_cast<float>(slider.getY() + slider.getPositionOfValue(m.db));
            if (y < trackTop - 2.0f || y > trackBottom + 2.0f)
                continue;

            const bool isUnity = (std::abs(m.db) < 0.01f);

            // Left tick mark
            const float leftTickLen = isUnity ? 7.0f : 4.0f;
            g.setColour(isUnity ? DSDLookAndFeel::getAccentAmber() : DSDLookAndFeel::getTextPrimary().withAlpha(0.65f));
            g.fillRect(trackCenterX - 18.0f - (isUnity ? 3.0f : 0.0f), y - 0.5f, leftTickLen, isUnity ? 1.5f : 1.0f);

            // Right tick mark (symmetrical parallel tick)
            const float rightTickLen = isUnity ? 7.0f : 4.0f;
            g.fillRect(trackCenterX + 18.0f, y - 0.5f, rightTickLen, isUnity ? 1.5f : 1.0f);

            // Number label (on left side, right-aligned to left tick)
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

            // Draw text right before the left tick
            g.drawText(m.label,
                       static_cast<int>(trackCenterX - 48.0f),
                       static_cast<int>(y - 6.0f),
                       26, 12,
                       juce::Justification::centredRight, false);
        }
    }
} // namespace dsd
