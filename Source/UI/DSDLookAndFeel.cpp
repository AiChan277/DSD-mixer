#include "UI/DSDLookAndFeel.h"

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
            return juce::FontOptions(12.5f, juce::Font::bold);
        return juce::FontOptions(std::clamp(static_cast<float>(buttonHeight) * 0.45f, 9.5f, 11.5f), juce::Font::bold);
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
        // 2. Broadcast Console Fader Cap (DHD RX2/SX2 Matte Chamfered Block)
        // ========================================================================
        const float capWidth = 32.0f;
        const float capHeight = 40.0f;
        const float capX = trackCenterX - capWidth * 0.5f;
        const float capY = sliderPos - capHeight * 0.5f;

        // Drop shadow under fader cap
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRoundedRectangle(capX - 2.0f, capY + 3.0f, capWidth + 4.0f, capHeight, 3.5f);

        // 3-stage vertical chamfer profile
        const float chamferH = 7.0f;

        // Top chamfer (specular reflection on top slope)
        auto topChamfer = juce::Rectangle<float>(capX, capY, capWidth, chamferH);
        juce::ColourGradient topGrad(juce::Colour(0xff3C4048), capX, capY,
                                     juce::Colour(0xff22252B), capX, capY + chamferH, false);
        g.setGradientFill(topGrad);
        g.fillRoundedRectangle(topChamfer, 2.5f);

        // Bottom chamfer (shadow on bottom slope)
        auto botChamfer = juce::Rectangle<float>(capX, capY + capHeight - chamferH, capWidth, chamferH);
        juce::ColourGradient botGrad(juce::Colour(0xff181A1E), capX, botChamfer.getY(),
                                     juce::Colour(0xff0D0E10), capX, botChamfer.getBottom(), false);
        g.setGradientFill(botGrad);
        g.fillRoundedRectangle(botChamfer, 2.5f);

        // Center main block: matte dark broadcast charcoal
        auto centerBlock = juce::Rectangle<float>(capX, capY + chamferH - 0.5f, capWidth, capHeight - 2.0f * chamferH + 1.0f);
        juce::ColourGradient midGrad(juce::Colour(0xff24272E), capX, centerBlock.getY(),
                                     juce::Colour(0xff181A1E), capX, centerBlock.getBottom(), false);
        g.setGradientFill(midGrad);
        g.fillRect(centerBlock);

        // Subtle tactile finger-grip ridges
        const float midY = capY + capHeight * 0.5f;
        g.setColour(juce::Colour(0xff121316));
        g.drawHorizontalLine(static_cast<int>(midY - 6.0f), capX + 4.0f, capX + capWidth - 4.0f);
        g.drawHorizontalLine(static_cast<int>(midY + 6.0f), capX + 4.0f, capX + capWidth - 4.0f);
        g.setColour(juce::Colour(0xff30343D));
        g.drawHorizontalLine(static_cast<int>(midY - 5.0f), capX + 4.0f, capX + capWidth - 4.0f);
        g.drawHorizontalLine(static_cast<int>(midY + 7.0f), capX + 4.0f, capX + capWidth - 4.0f);

        // Fader cap outer border
        g.setColour(juce::Colour(0xff40444D));
        g.drawRoundedRectangle(capX, capY, capWidth, capHeight, 2.5f, 1.0f);

        // Top edge highlight hairline
        g.setColour(juce::Colour(0xff5E6472));
        g.drawHorizontalLine(static_cast<int>(capY + 0.5f), capX + 2.0f, capX + capWidth - 2.0f);

        // 3. Center White Position Indicator Line (Crisp 2.0px Pure White)
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(capX + 1.5f, midY, capWidth - 3.0f, 2.0f);
        g.setColour(juce::Colours::white);
        g.fillRect(capX + 1.5f, midY - 1.0f, capWidth - 3.0f, 2.0f);
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
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isToggled = button.getToggleState();
        const float cornerRadius = 3.5f;

        // 1. Recessed bezel seating border around button
        g.setColour(juce::Colour(0xff7D818A));
        g.drawRoundedRectangle(bounds, cornerRadius, 1.2f);

        auto innerBounds = bounds.reduced(1.0f);
        if (shouldDrawButtonAsDown)
            innerBounds = innerBounds.translated(0.0f, 0.8f);

        if (isToggled)
        {
            // ====================================================================
            // ON STATE: ILLUMINATED INCANDESCENT BULB WITH FILM LAYER
            // ====================================================================
            const bool isRedAlert = button.getButtonText().equalsIgnoreCase("ON") 
                                 || button.getButtonText().equalsIgnoreCase("MUTE") 
                                 || button.getButtonText().equalsIgnoreCase("OFF");
            const float cx = innerBounds.getCentreX();
            const float cy = innerBounds.getCentreY();

            // Radial incandescent bulb gradient: Glowing warm core -> rich bulb -> deep edge
            juce::Colour bulbCore = isRedAlert ? juce::Colour(0xffff5722) : getAmberBulbCore();
            juce::Colour bulbMid  = isRedAlert ? juce::Colour(0xffd50000) : getAmberBulbMid();
            juce::Colour bulbEdge = isRedAlert ? juce::Colour(0xff8a0000) : getAmberBulbEdge();

            if (shouldDrawButtonAsHighlighted)
            {
                bulbCore = bulbCore.brighter(0.12f);
                bulbMid  = bulbMid.brighter(0.08f);
            }

            juce::ColourGradient bulbGlow(bulbCore, cx, cy - 2.0f,
                                          bulbEdge, cx, innerBounds.getBottom(), true);
            bulbGlow.addColour(0.40, bulbMid);
            g.setGradientFill(bulbGlow);
            g.fillRoundedRectangle(innerBounds, cornerRadius);

            // Internal translucent film layer: soft milky diffusion
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(innerBounds.reduced(1.0f), cornerRadius);

            // Protective acrylic cap highlight (specular film meniscus on upper 45%)
            auto highlightRect = innerBounds.removeFromTop(innerBounds.getHeight() * 0.48f).reduced(1.0f, 0.5f);
            juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.40f), highlightRect.getX(), highlightRect.getY(),
                                           juce::Colours::white.withAlpha(0.04f), highlightRect.getX(), highlightRect.getBottom(), false);
            g.setGradientFill(glassGrad);
            g.fillRoundedRectangle(highlightRect, 2.0f);

            // Outer illuminated lens rim glow
            g.setColour((isRedAlert ? juce::Colour(0xffff8a80) : juce::Colour(0xffffe082)).withAlpha(0.75f));
            g.drawRoundedRectangle(innerBounds, cornerRadius, 1.0f);

            // Subtle warm ambient light bleed on bezel
            g.setColour((isRedAlert ? juce::Colour(0xffff1744) : juce::Colour(0xffffa000)).withAlpha(0.35f));
            g.drawRoundedRectangle(bounds.expanded(0.5f), cornerRadius + 0.5f, 0.8f);
        }
        else
        {
            // ====================================================================
            // OFF STATE: TRANSLUCENT MILKY FROSTED CAP (UNLIT)
            // ====================================================================
            juce::Colour capTop = shouldDrawButtonAsHighlighted ? juce::Colour(0xffECEEF2) : juce::Colour(0xffDFE2E8);
            juce::Colour capBot = shouldDrawButtonAsHighlighted ? juce::Colour(0xffD6D9E0) : juce::Colour(0xffCBD0D8);

            if (shouldDrawButtonAsDown)
                std::swap(capTop, capBot);

            juce::ColourGradient capGrad(capTop, innerBounds.getX(), innerBounds.getY(),
                                         capBot, innerBounds.getX(), innerBounds.getBottom(), false);
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(innerBounds, cornerRadius);

            // Subtle glossy film sheen across upper 42%
            auto sheenRect = innerBounds.removeFromTop(innerBounds.getHeight() * 0.42f).reduced(1.0f, 0.5f);
            juce::ColourGradient sheenGrad(juce::Colours::white.withAlpha(0.28f), sheenRect.getX(), sheenRect.getY(),
                                           juce::Colours::white.withAlpha(0.02f), sheenRect.getX(), sheenRect.getBottom(), false);
            g.setGradientFill(sheenGrad);
            g.fillRoundedRectangle(sheenRect, 2.0f);

            // Bevel rim (clean broadcast button edge)
            g.setColour(juce::Colour(0xffB4B8C2));
            g.drawRoundedRectangle(innerBounds, cornerRadius, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(innerBounds.getY() + 1.0f), innerBounds.getX() + 2.0f, innerBounds.getRight() - 2.0f);
        }
    }

    void DSDLookAndFeel::drawButtonText(juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
    {
        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);

        const bool isToggled = button.getToggleState();
        auto bounds = button.getLocalBounds().toFloat();

        if (isToggled)
        {
            const bool isRedAlert = button.getButtonText().equalsIgnoreCase("ON") 
                                 || button.getButtonText().equalsIgnoreCase("MUTE") 
                                 || button.getButtonText().equalsIgnoreCase("OFF");
            if (isRedAlert)
            {
                // Crisp high-contrast white text on glowing red broadcast lens
                g.setColour(juce::Colours::black.withAlpha(0.55f));
                g.drawFittedText(button.getButtonText(),
                                 bounds.translated(0.0f, 1.0f).toNearestInt(),
                                 juce::Justification::centred, 1);

                g.setColour(juce::Colours::white);
                g.drawFittedText(button.getButtonText(),
                                 bounds.toNearestInt(),
                                 juce::Justification::centred, 1);
            }
            else
            {
                // Backlit film stencil lettering (crisp dark silhouette on glowing amber bulb)
                g.setColour(juce::Colour(0xff3e1800).withAlpha(0.40f));
                g.drawFittedText(button.getButtonText(),
                                 bounds.translated(0.0f, 1.0f).toNearestInt(),
                                 juce::Justification::centred, 1);

                g.setColour(juce::Colour(0xff180800));
                g.drawFittedText(button.getButtonText(),
                                 bounds.toNearestInt(),
                                 juce::Justification::centred, 1);
            }
        }
        else
        {
            // Crisp dark graphite printed legend on milky film
            g.setColour(juce::Colour(0xff2A2E35));
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
