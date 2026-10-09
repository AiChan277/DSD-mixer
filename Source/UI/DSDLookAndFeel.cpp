#include "UI/DSDLookAndFeel.h"
#include <BinaryData.h>

namespace dsd
{
    DSDLookAndFeel::DSDLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, getConsoleDarkBg());
        setColour(juce::Label::textColourId, getTextPrimary());
        setColour(juce::TextButton::textColourOffId, getTextPrimary());
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff180800)); // Dark silhouette on illuminated amber
        setColour(juce::ComboBox::backgroundColourId, getOledBlack());
        setColour(juce::ComboBox::textColourId, getTextOnOled());
        setColour(juce::ComboBox::outlineColourId, getConsoleBevel());
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1C1E23));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffE2E5EB));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffff9100));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff1A0A00));
    }

    juce::Font DSDLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
    {
        if (button.getButtonText().equalsIgnoreCase("ON"))
            return juce::FontOptions(12.0f, juce::Font::bold);
        return juce::FontOptions(std::clamp(static_cast<float>(buttonHeight) * 0.38f, 9.0f, 10.5f), juce::Font::bold);
    }

    void DSDLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                         const juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
    {
        if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
            return;

        const float trackCenterX = std::floor(static_cast<float>(x) + static_cast<float>(width) * 0.50f);
        const float trackWidth = 5.0f;
        const float trackX = trackCenterX - trackWidth * 0.5f;

        const float topPos = static_cast<float>(slider.getPositionOfValue(slider.getMaximum()));
        const float botPos = static_cast<float>(slider.getPositionOfValue(slider.getMinimum()));
        const float trackY = std::min(topPos, botPos);
        const float trackHeight = std::abs(botPos - topPos);

        // 1. Draw recessed dark track slot on light console chassis
        g.setColour(juce::Colour(0xff14161A));
        g.fillRoundedRectangle(trackX - 0.5f, trackY - 1.0f, trackWidth + 1.0f, trackHeight + 2.0f, 2.0f);

        g.setColour(juce::Colour(0xff22252C));
        g.fillRoundedRectangle(trackX, trackY, trackWidth, trackHeight, 1.5f);

        // Center silver hairline guide
        g.setColour(juce::Colour(0xff555963));
        g.drawVerticalLine(static_cast<int>(trackCenterX), trackY + 1.0f, trackY + trackHeight - 1.0f);

        // ========================================================================
        // 2. Fader thumb — render Fader.png asset centered on sliderPos
        // ========================================================================
        // 2. Fader thumb — render Fader.png asset centered on sliderPos
        // Fader.png is 157x327 (ratio 0.48). Width 25px -> Height 52px (exact proportion)
        const float capWidth  = 25.0f;
        const float capHeight = 52.0f;
        const float capX = trackCenterX - capWidth * 0.5f;
        const float capY = sliderPos - capHeight * 0.5f;

        auto faderImg = juce::ImageCache::getFromMemory(
                            BinaryData::Fader_png, BinaryData::Fader_pngSize);

        if (faderImg.isValid())
        {
            // Draw the PNG thumb directly with no second thumb or generic artifacts
            g.drawImage(faderImg,
                        static_cast<int>(capX), static_cast<int>(capY),
                        static_cast<int>(capWidth), static_cast<int>(capHeight),
                        0, 0, faderImg.getWidth(), faderImg.getHeight());
        }
        else
        {
            // Fallback: drawn cap if image unavailable
            const float midY = capY + capHeight * 0.5f;

            g.setColour(juce::Colours::black.withAlpha(0.38f));
            g.fillRoundedRectangle(capX - 2.0f, capY + 3.0f, capWidth + 4.0f, capHeight, 3.5f);

            juce::ColourGradient midGrad(juce::Colour(0xff24272E), capX, capY,
                                         juce::Colour(0xff181A1E), capX, capY + capHeight, false);
            g.setGradientFill(midGrad);
            g.fillRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f);

            g.setColour(juce::Colour(0xff40444D));
            g.drawRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f, 1.0f);

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRect(capX + 1.5f, midY, capWidth - 3.0f, 2.0f);
            g.setColour(juce::Colours::white);
            g.fillRect(capX + 1.5f, midY - 1.0f, capWidth - 3.0f, 2.0f);
        }
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
        g.setColour(juce::Colour(0xff2A2D33));
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc (Amber illuminated)
        juce::Path valuePath;
        valuePath.addCentredArc(centreX, centreY, radius + 2.0f, radius + 2.0f, 0.0f,
                                rotaryStartAngle, currentAngle, true);
        g.setColour(getAccentAmber());
        g.strokePath(valuePath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Dial body
        juce::ColourGradient knobGrad(juce::Colour(0xff7A7E88), centreX - radius, centreY - radius,
                                      juce::Colour(0xff4A4D55), centreX + radius, centreY + radius, true);
        g.setGradientFill(knobGrad);
        g.fillEllipse(rx, ry, rw, rw);

        g.setColour(juce::Colour(0xff9DA2AD));
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
                                              const juce::Colour& /*backgroundColour*/,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat();
        if (shouldDrawButtonAsDown)
            bounds = bounds.translated(0.0f, 0.8f);

        const bool isLit = button.getToggleState() || shouldDrawButtonAsDown;

        auto img = isLit ? juce::ImageCache::getFromMemory(BinaryData::OnButton_png, BinaryData::OnButton_pngSize)
                         : juce::ImageCache::getFromMemory(BinaryData::OffButton_png, BinaryData::OffButton_pngSize);

        if (img.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

            const int bx = static_cast<int>(std::round(bounds.getX()));
            const int by = static_cast<int>(std::round(bounds.getY()));
            const int bw = static_cast<int>(std::round(bounds.getWidth()));
            const int bh = static_cast<int>(std::round(bounds.getHeight()));

            if (bw > 0 && bh > 0)
            {
                const int sw = img.getWidth();
                const int sh = img.getHeight();
                // Bezel + inner rounded corner is within the outer 76px of the 307x302 asset
                const int sc = 76;
                const int minDim = std::min(bw, bh);
                const int dc = std::clamp(static_cast<int>(std::round(static_cast<float>(minDim) * (static_cast<float>(sc) / static_cast<float>(sh)))),
                                          2, std::max(2, minDim / 2));

                const int dwMid = std::max(0, bw - 2 * dc);
                const int dhMid = std::max(0, bh - 2 * dc);
                const int swMid = sw - 2 * sc;
                const int shMid = sh - 2 * sc;

                // 4 Corners
                g.drawImage(img, bx,               by,               dc, dc, 0,       0,       sc, sc);
                g.drawImage(img, bx + bw - dc,     by,               dc, dc, sw - sc, 0,       sc, sc);
                g.drawImage(img, bx,               by + bh - dc,     dc, dc, 0,       sh - sc, sc, sc);
                g.drawImage(img, bx + bw - dc,     by + bh - dc,     dc, dc, sw - sc, sh - sc, sc, sc);

                // Top & Bottom Edges
                if (dwMid > 0)
                {
                    g.drawImage(img, bx + dc, by,           dwMid, dc, sc, 0,       swMid, sc);
                    g.drawImage(img, bx + dc, by + bh - dc, dwMid, dc, sc, sh - sc, swMid, sc);
                }

                // Left & Right Edges
                if (dhMid > 0)
                {
                    g.drawImage(img, bx,           by + dc, dc, dhMid, 0,       sc, sc, shMid);
                    g.drawImage(img, bx + bw - dc, by + dc, dc, dhMid, sw - sc, sc, sc, shMid);
                }

                // Center Cap
                if (dwMid > 0 && dhMid > 0)
                {
                    g.drawImage(img, bx + dc, by + dc, dwMid, dhMid, sc, sc, swMid, shMid);
                }

                if (shouldDrawButtonAsHighlighted && button.isEnabled())
                {
                    auto innerCap = bounds.reduced(static_cast<float>(dc) * 0.65f);
                    g.setColour(juce::Colours::white.withAlpha(isLit ? 0.10f : 0.16f));
                    g.fillRoundedRectangle(innerCap, 2.0f);
                }

                if (!button.isEnabled())
                {
                    g.setColour(juce::Colours::black.withAlpha(0.35f));
                    g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
                }
            }
            return;
        }

        // Vector fallback if asset is unavailable
        const float cornerRadius = 3.5f;
        g.setColour(juce::Colour(0xff7D818A));
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 1.2f);
        auto innerBounds = bounds.reduced(2.0f);
        g.setColour(isLit ? getAccentAmber() : juce::Colour(0xffCBD0D8));
        g.fillRoundedRectangle(innerBounds, cornerRadius);
    }

    void DSDLookAndFeel::drawButtonText(juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool shouldDrawButtonAsDown)
    {
        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);

        const bool isLit = button.getToggleState() || shouldDrawButtonAsDown;
        auto bounds = button.getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        if (shouldDrawButtonAsDown)
            bounds = bounds.translated(0.0f, 0.8f);

        if (isLit)
        {
            // Dark silhouette lettering on glowing amber OnButton.png cap
            g.setColour(juce::Colour(0xff3e1800).withAlpha(0.35f));
            g.drawFittedText(button.getButtonText(),
                             bounds.translated(0.0f, 1.0f).toNearestInt(),
                             juce::Justification::centred, 1);
            g.setColour(juce::Colour(0xff180800));
            g.drawFittedText(button.getButtonText(),
                             bounds.toNearestInt(),
                             juce::Justification::centred, 1);
        }
        else
        {
            // Crisp dark graphite printed legend on milky OffButton.png cap
            g.setColour(button.isEnabled() ? juce::Colour(0xff22262C) : juce::Colour(0xff6B707B));
            g.drawFittedText(button.getButtonText(),
                             bounds.toNearestInt(),
                             juce::Justification::centred, 1);
        }
    }

    void DSDLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& /*box*/)
    {
        const float cornerRadius = 3.0f;
        juce::Rectangle<float> bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        bounds = bounds.reduced(0.5f);

        // Dark recessed OLED selector slot
        g.setColour(getOledBlack());
        g.fillRoundedRectangle(bounds, cornerRadius);

        // Border with bevel
        g.setColour(getConsoleBevel());
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

        // Draw small triangle arrow on right side
        const float arrowX = buttonW > 0 ? static_cast<float>(buttonX) : static_cast<float>(width - 16);
        const float arrowY = buttonH > 0 ? static_cast<float>(buttonY) : 0.0f;
        const float arrowW = buttonW > 0 ? static_cast<float>(buttonW) : 14.0f;
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
