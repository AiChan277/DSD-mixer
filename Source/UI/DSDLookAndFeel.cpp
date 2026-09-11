#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    DSDLookAndFeel::DSDLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, getConsoleDarkBg());
        setColour(juce::Label::textColourId, getTextPrimary());
        setColour(juce::TextButton::textColourOffId, getTextPrimary());
        setColour(juce::TextButton::textColourOnId, getTextPrimary());
    }

    void DSDLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                         const juce::Slider::SliderStyle style,
                                         juce::Slider& /*slider*/)
    {
        if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
            return;

        const float trackWidth = 6.0f;
        const float trackX = static_cast<float>(x) + (static_cast<float>(width) - trackWidth) * 0.5f;
        const float trackY = static_cast<float>(y) + 10.0f;
        const float trackHeight = static_cast<float>(height) - 20.0f;

        // Draw recessed track slot
        g.setColour(juce::Colour(0xff141518));
        g.fillRoundedRectangle(trackX, trackY, trackWidth, trackHeight, 3.0f);

        // Center silver hairline guide
        g.setColour(juce::Colour(0xff4b5563));
        g.drawVerticalLine(static_cast<int>(trackX + trackWidth * 0.5f), trackY + 2.0f, trackY + trackHeight - 2.0f);

        // Draw professional broadcast fader cap (metal cap with center indicator)
        const float capWidth = static_cast<float>(width) * 0.70f;
        const float capHeight = 32.0f;
        const float capX = static_cast<float>(x) + (static_cast<float>(width) - capWidth) * 0.5f;
        const float capY = std::clamp(sliderPos - (capHeight * 0.5f), trackY, trackY + trackHeight - capHeight);

        // Fader cap drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(capX - 1.0f, capY + 2.0f, capWidth + 2.0f, capHeight, 3.0f);

        // Fader cap body gradient (metallic brushed finish)
        juce::ColourGradient capGrad(juce::Colour(0xff3f444e), capX, capY,
                                     juce::Colour(0xff22252a), capX, capY + capHeight, false);
        g.setGradientFill(capGrad);
        g.fillRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f);

        // Cap bevel edge
        g.setColour(juce::Colour(0xff525866));
        g.drawRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f, 1.2f);

        // Center white indicator stripe
        g.setColour(juce::Colour(0xffffffff));
        g.fillRect(capX + 3.0f, capY + (capHeight * 0.5f) - 1.0f, capWidth - 6.0f, 2.0f);
    }

    void DSDLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& /*slider*/)
    {
        const float radius = static_cast<float>(std::min(width, height)) * 0.45f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float rx = centreX - radius;
        const float ry = centreY - radius;
        const float rw = radius * 2.0f;

        const float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Outer arc track
        juce::Path trackPath;
        trackPath.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f,
                                rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1a1c20));
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc
        juce::Path valuePath;
        valuePath.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f,
                                rotaryStartAngle, currentAngle, true);
        g.setColour(getAccentBlue());
        g.strokePath(valuePath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Dial body
        juce::ColourGradient knobGrad(juce::Colour(0xff373c44), centreX - radius, centreY - radius,
                                      juce::Colour(0xff1e2025), centreX + radius, centreY + radius, true);
        g.setGradientFill(knobGrad);
        g.fillEllipse(rx, ry, rw, rw);

        g.setColour(juce::Colour(0xff4a515d));
        g.drawEllipse(rx, ry, rw, rw, 1.2f);

        // Pointer notch
        juce::Path p;
        const float pointerLength = radius * 0.65f;
        p.addRectangle(-1.5f, -radius, 3.0f, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(currentAngle).translated(centreX, centreY));
        g.setColour(getTextPrimary());
        g.fillPath(p);
    }

    void DSDLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isToggled = button.getToggleState();

        juce::Colour fillCol = backgroundColour;
        if (isToggled)
        {
            // Illuminated LED glow
            fillCol = backgroundColour.brighter(0.25f);
        }
        else
        {
            fillCol = backgroundColour.darker(0.35f);
        }

        if (shouldDrawButtonAsDown)
            fillCol = fillCol.darker(0.15f);
        else if (shouldDrawButtonAsHighlighted)
            fillCol = fillCol.brighter(0.10f);

        // Shadow & button body
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f), 4.0f);

        g.setColour(fillCol);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Bevel border
        g.setColour(isToggled ? fillCol.brighter(0.4f) : juce::Colour(0xff4b5563));
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

        // If toggled, small illuminated top glow line
        if (isToggled)
        {
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillRect(bounds.getX() + 4.0f, bounds.getY() + 2.0f, bounds.getWidth() - 8.0f, 1.5f);
        }
    }
} // namespace dsd
