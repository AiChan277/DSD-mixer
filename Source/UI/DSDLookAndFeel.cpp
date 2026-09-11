#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    DSDLookAndFeel::DSDLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, getConsoleDarkBg());
        setColour(juce::Label::textColourId, getTextPrimary());
        setColour(juce::TextButton::textColourOffId, getTextPrimary());
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffFFFFFF)); // white text on illuminated buttons
        setColour(juce::ComboBox::backgroundColourId, getOledBlack());
        setColour(juce::ComboBox::textColourId, getTextOnOled());
        setColour(juce::ComboBox::outlineColourId, getConsoleBevel());
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff2A2D33));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffE8EAED));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, getAccentBlue());
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
        g.setColour(juce::Colour(0xff3A3D42));
        g.fillRoundedRectangle(trackX, trackY, trackWidth, trackHeight, 3.0f);

        // Center silver hairline guide
        g.setColour(juce::Colour(0xff6B6F78));
        g.drawVerticalLine(static_cast<int>(trackX + trackWidth * 0.5f), trackY + 2.0f, trackY + trackHeight - 2.0f);

        // Draw professional broadcast fader cap (metal cap with center indicator)
        const float capWidth = static_cast<float>(width) * 0.70f;
        const float capHeight = 32.0f;
        const float capX = static_cast<float>(x) + (static_cast<float>(width) - capWidth) * 0.5f;
        const float capY = std::clamp(sliderPos - (capHeight * 0.5f), trackY, trackY + trackHeight - capHeight);

        // Fader cap drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(capX - 1.0f, capY + 2.0f, capWidth + 2.0f, capHeight, 3.0f);

        // Fader cap body gradient (metallic brushed finish)
        juce::ColourGradient capGrad(juce::Colour(0xff8A8D95), capX, capY,
                                     juce::Colour(0xff5A5D64), capX, capY + capHeight, false);
        g.setGradientFill(capGrad);
        g.fillRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f);

        // Cap bevel edge
        g.setColour(juce::Colour(0xffA0A4AB));
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
        g.setColour(juce::Colour(0xff3A3D42));
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc
        juce::Path valuePath;
        valuePath.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f,
                                rotaryStartAngle, currentAngle, true);
        g.setColour(getAccentBlue());
        g.strokePath(valuePath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Dial body gradient
        juce::ColourGradient knobGrad(juce::Colour(0xff8A8D95), centreX - radius, centreY - radius,
                                      juce::Colour(0xff5A5D64), centreX + radius, centreY + radius, true);
        g.setGradientFill(knobGrad);
        g.fillEllipse(rx, ry, rw, rw);

        // Border
        g.setColour(juce::Colour(0xffA0A4AB));
        g.drawEllipse(rx, ry, rw, rw, 1.2f);

        // Pointer notch
        juce::Path p;
        const float pointerLength = radius * 0.65f;
        p.addRectangle(-1.5f, -radius, 3.0f, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(currentAngle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xff1A1C20));
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

        juce::Colour fillCol = isToggled ? backgroundColour : getButtonOffBg();

        if (shouldDrawButtonAsDown)
            fillCol = fillCol.darker(0.12f);
        else if (shouldDrawButtonAsHighlighted)
            fillCol = fillCol.brighter(0.06f);

        // Subtle shadow
        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 2.5f);

        // Button body
        g.setColour(fillCol);
        g.fillRoundedRectangle(bounds, 2.5f);

        // Bevel border
        g.setColour(isToggled ? fillCol.brighter(0.35f) : getConsoleBevel());
        g.drawRoundedRectangle(bounds, 2.5f, 1.0f);

        // If toggled ON: small illuminated top glow line
        if (isToggled)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillRect(bounds.getX() + 3.0f, bounds.getY() + 1.5f, bounds.getWidth() - 6.0f, 1.5f);
        }
    }

    void DSDLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& /*box*/)
    {
        const float cornerRadius = 3.0f;
        juce::Rectangle<float> bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        bounds = bounds.reduced(0.5f);

        // Fill with getOledBlack()
        g.setColour(getOledBlack());
        g.fillRoundedRectangle(bounds, cornerRadius);

        // Border with getConsoleBevel()
        g.setColour(getConsoleBevel());
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

        // Draw small triangle arrow on right side using getTextOnOled()
        const float arrowX = buttonW > 0 ? static_cast<float>(buttonX) : static_cast<float>(width - 18);
        const float arrowY = buttonH > 0 ? static_cast<float>(buttonY) : 0.0f;
        const float arrowW = buttonW > 0 ? static_cast<float>(buttonW) : 16.0f;
        const float arrowH = buttonH > 0 ? static_cast<float>(buttonH) : static_cast<float>(height);

        juce::Rectangle<float> arrowBounds(arrowX, arrowY, arrowW, arrowH);
        const float centreX = arrowBounds.getCentreX();
        const float centreY = arrowBounds.getCentreY();
        const float halfW = 3.5f;
        const float halfH = 2.5f;

        juce::Path arrow;
        arrow.addTriangle(centreX - halfW, centreY - halfH,
                          centreX + halfW, centreY - halfH,
                          centreX, centreY + halfH);

        g.setColour(getTextOnOled());
        g.fillPath(arrow);
    }
} // namespace dsd
