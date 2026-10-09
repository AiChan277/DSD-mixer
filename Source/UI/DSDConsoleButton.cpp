#include "DSDConsoleButton.h"
#include <BinaryData.h>

namespace dsd
{

DSDConsoleButton::DSDConsoleButton(const juce::String& text,
                                   ConsoleButtonStyle style,
                                   ButtonLabelPlacement placement)
    : label(text), buttonStyle(style), labelPlacement(placement)
{
}

void DSDConsoleButton::setToggled(bool state, bool sendCallback)
{
    if (toggled != state)
    {
        toggled = state;
        repaint();
        if (sendCallback && onToggled)
            onToggled(toggled);
    }
}

void DSDConsoleButton::setButtonEnabled(bool enabled)
{
    if (buttonEnabled != enabled)
    {
        buttonEnabled = enabled;
        repaint();
    }
}

void DSDConsoleButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (buttonStyle == ConsoleButtonStyle::RectangularVst || buttonStyle == ConsoleButtonStyle::RectangularDm)
    {
        // Rectangular buttons: VST / DM
        const char* data = (buttonStyle == ConsoleButtonStyle::RectangularVst) ? BinaryData::BtnVst_png : BinaryData::BtnDm_png;
        int size = (buttonStyle == ConsoleButtonStyle::RectangularVst) ? BinaryData::BtnVst_pngSize : BinaryData::BtnDm_pngSize;
        auto img = juce::ImageCache::getFromMemory(data, size);

        // Aspect ratio of BtnVst/BtnDm is 270x120 = 2.25
        float maxW = bounds.getWidth();
        float maxH = bounds.getHeight();
        float targetW = maxW;
        float targetH = targetW / 2.25f;
        if (targetH > maxH)
        {
            targetH = maxH;
            targetW = targetH * 2.25f;
        }

        auto btnArea = juce::Rectangle<float>(bounds.getCentreX() - targetW * 0.5f,
                                              bounds.getCentreY() - targetH * 0.5f,
                                              targetW, targetH);
        if (mouseIsDown)
            btnArea.translate(0.0f, 0.8f);

        if (img.isValid())
        {
            g.drawImage(img, btnArea);
        }
        else
        {
            g.setColour(juce::Colour(0xffCBD0D8));
            g.fillRoundedRectangle(btnArea, 3.5f);
            g.setColour(juce::Colour(0xff7D818A));
            g.drawRoundedRectangle(btnArea, 3.5f, 1.0f);
            g.setColour(juce::Colour(0xff2A2D33));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(label, btnArea, juce::Justification::centred, false);
        }

        // Active state indicator for DM
        if (toggled)
        {
            g.setColour(juce::Colour(0xffFFA000));
            g.drawRoundedRectangle(btnArea.reduced(0.5f), 3.0f, 1.5f);
        }

        if (!buttonEnabled)
        {
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(btnArea, 3.0f);
        }
        return;
    }

    // Square buttons: MUTE, PH, MONO, PFL, HPF, PAD, ON, OFF
    // Uses supplied OnButton.png / OffButton.png directly
    juce::Rectangle<float> labelArea;
    juce::Rectangle<float> btnArea;

    if (labelPlacement == ButtonLabelPlacement::Above)
    {
        labelArea = bounds.removeFromTop(12.0f);
        float side = std::min(bounds.getWidth(), bounds.getHeight());
        btnArea = juce::Rectangle<float>(bounds.getCentreX() - side * 0.5f,
                                          bounds.getY() + (bounds.getHeight() - side) * 0.5f,
                                          side, side);
    }
    else if (labelPlacement == ButtonLabelPlacement::Below)
    {
        labelArea = bounds.removeFromBottom(12.0f);
        float side = std::min(bounds.getWidth(), bounds.getHeight());
        btnArea = juce::Rectangle<float>(bounds.getCentreX() - side * 0.5f,
                                          bounds.getY() + (bounds.getHeight() - side) * 0.5f,
                                          side, side);
    }
    else
    {
        float side = std::min(bounds.getWidth(), bounds.getHeight());
        btnArea = juce::Rectangle<float>(bounds.getCentreX() - side * 0.5f,
                                          bounds.getCentreY() - side * 0.5f,
                                          side, side);
    }

    if (mouseIsDown)
        btnArea.translate(0.0f, 0.8f);

    auto img = toggled ? juce::ImageCache::getFromMemory(BinaryData::OnButton_png, BinaryData::OnButton_pngSize)
                       : juce::ImageCache::getFromMemory(BinaryData::OffButton_png, BinaryData::OffButton_pngSize);

    if (img.isValid())
    {
        g.drawImage(img, btnArea);
    }
    else
    {
        g.setColour(toggled ? juce::Colour(0xffFFA000) : juce::Colour(0xffCBD0D8));
        g.fillRoundedRectangle(btnArea, 3.0f);
        g.setColour(juce::Colour(0xff7D818A));
        g.drawRoundedRectangle(btnArea, 3.0f, 1.0f);
    }

    if (!buttonEnabled)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(btnArea, 3.0f);
    }

    // Draw label
    if (label.isNotEmpty() && labelPlacement != ButtonLabelPlacement::None && labelPlacement != ButtonLabelPlacement::Inside)
    {
        g.setColour(juce::Colour(0xff2A2D33));
        if (label == "PAD")
        {
            // "PAD" bold, followed by "-20dB" in smaller font
            const float btnCenterX = btnArea.getCentreX();

            g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
            g.drawText("PAD", static_cast<int>(btnCenterX - 18.0f), static_cast<int>(labelArea.getY()),
                       24, static_cast<int>(labelArea.getHeight()),
                       juce::Justification::centred, false);

            g.setFont(juce::FontOptions(6.0f));
            g.drawText("-20dB", static_cast<int>(btnCenterX + 5.0f), static_cast<int>(labelArea.getY() + 1.0f),
                       20, static_cast<int>(labelArea.getHeight()),
                       juce::Justification::centredLeft, false);
        }
        else
        {
            g.setFont(juce::FontOptions(labelPlacement == ButtonLabelPlacement::Above ? 10.0f : 9.0f, juce::Font::bold));
            g.drawText(label, labelArea, juce::Justification::centred, false);
        }
    }
}

void DSDConsoleButton::mouseDown(const juce::MouseEvent& /*e*/)
{
    mouseIsDown = true;
    repaint();
}

void DSDConsoleButton::mouseUp(const juce::MouseEvent& e)
{
    mouseIsDown = false;

    if (buttonEnabled && getLocalBounds().contains(e.getPosition()))
    {
        if (toggleMode)
            toggled = !toggled;
        if (onToggled)
            onToggled(toggled);
        if (onClick)
            onClick();
    }

    repaint();
}

} // namespace dsd
