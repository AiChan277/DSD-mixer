#include "DSDKnob.h"
#include <BinaryData.h>

namespace dsd
{

DSDKnob::DSDKnob(const juce::String& labelText, KnobStyle style, double minVal, double maxVal, double defaultVal)
    : label(labelText), knobStyle(style), value(defaultVal), minValue(minVal), maxValue(maxVal), defaultValue(defaultVal)
{
    formatValue = [this](double v) -> juce::String
    {
        if (knobStyle == KnobStyle::PanKnob)
        {
            if (std::abs(v) < 0.02)
                return "C";
            if (v < 0)
                return "L " + juce::String(static_cast<int>(std::round(-v * 100)));
            return "R " + juce::String(static_cast<int>(std::round(v * 100)));
        }
        else
        {
            if (v <= minValue + 0.5)
                return "-inf";
            return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
        }
    };
}

void DSDKnob::setValue(double newValue, bool sendCallback)
{
    newValue = juce::jlimit(minValue, maxValue, newValue);
    if (value != newValue)
    {
        value = newValue;
        repaint();
        if (sendCallback && onValueChanged)
            onValueChanged(value);
    }
}

void DSDKnob::setRange(double minVal, double maxVal, double defaultVal)
{
    minValue = minVal;
    maxValue = maxVal;
    defaultValue = defaultVal;
    value = juce::jlimit(minValue, maxValue, value);
    repaint();
}

double DSDKnob::getNormalisedValue() const
{
    if (maxValue <= minValue)
        return 0.0;
    return (value - minValue) / (maxValue - minValue);
}

void DSDKnob::setNormalisedValue(double norm, bool sendCallback)
{
    norm = juce::jlimit(0.0, 1.0, norm);
    double newValue = minValue + norm * (maxValue - minValue);
    newValue = juce::jlimit(minValue, maxValue, newValue);

    if (value != newValue)
    {
        value = newValue;
        repaint();
        if (sendCallback && onValueChanged)
            onValueChanged(value);
    }
}

void DSDKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float knobDiameter = 36.0f;
    const float knobRadius = knobDiameter * 0.5f;

    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();

    if (knobStyle == KnobStyle::GainKnob)
    {
        // ── 1. Top row: GAIN label (left) + dark numeric pill (right) ──
        auto topRow = bounds.removeFromTop(14.0f);

        // GAIN label on top-left
        g.setColour(juce::Colour(0xff2A2D33));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("GAIN", topRow.removeFromLeft(40.0f), juce::Justification::centredLeft, false);

        // Dark pill on top-right: e.g. "0.0 dB"
        auto pillRect = topRow.removeFromRight(46.0f).withHeight(13.0f).translated(0.0f, 0.5f);
        g.setColour(juce::Colour(0xff121418));
        g.fillRoundedRectangle(pillRect, 2.5f);
        g.setColour(juce::Colour(0xff30343C));
        g.drawRoundedRectangle(pillRect, 2.5f, 0.8f);

        // Amber readout text
        juce::String valText = formatValue ? formatValue(value) : juce::String(value, 1) + " dB";
        g.setColour(juce::Colour(0xffFFA000));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(valText, pillRect, juce::Justification::centred, false);

        // Center knob in remaining area
        centreX = bounds.getX() + bounds.getWidth() * 0.40f;
        centreY = bounds.getY() + 22.0f;
    }
    else
    {
        // PanKnob: Knob centered in top area, PAN label below
        centreX = bounds.getCentreX();
        centreY = bounds.getY() + 20.0f;

        auto labelArea = bounds.removeFromBottom(12.0f);
        g.setColour(juce::Colour(0xff2A2D33));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("PAN", labelArea, juce::Justification::centred, false);
    }

    // ── 2. Surrounding Dot Ring ──
    const float dotRadius = knobRadius + 5.0f;
    constexpr float maxAngle = juce::MathConstants<float>::pi * 0.75f; // ±135 degrees
    constexpr int numDots = 21;

    for (int i = 0; i < numDots; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numDots - 1);
        float a = -maxAngle + t * (2.0f * maxAngle);
        float dx = centreX + dotRadius * std::sin(a);
        float dy = centreY - dotRadius * std::cos(a);

        // Key positions: center (12 o'clock, i=10), 9 o'clock (i=3), 3 o'clock (i=17)
        bool isCenter = (i == 10);
        bool isMajor = (i == 3 || i == 17);

        float r = isCenter ? 1.7f : (isMajor ? 1.4f : 0.85f);
        g.setColour(juce::Colour(0xff2A2D33));
        g.fillEllipse(dx - r, dy - r, r * 2.0f, r * 2.0f);
    }

    // Tick labels -20 and +20 for GAIN
    if (knobStyle == KnobStyle::GainKnob)
    {
        g.setFont(juce::FontOptions(8.0f));
        g.setColour(juce::Colour(0xff2A2D33));
        g.drawText("-20", static_cast<int>(centreX - dotRadius - 15.0f), static_cast<int>(centreY + 8.0f), 18, 10, juce::Justification::centred, false);
        g.drawText("+20", static_cast<int>(centreX + dotRadius - 3.0f), static_cast<int>(centreY + 8.0f), 18, 10, juce::Justification::centred, false);
    }

    // ── 3. Render Knob.png Rotated ──
    auto knobImg = juce::ImageCache::getFromMemory(BinaryData::Knob_png, BinaryData::Knob_pngSize);

    // Calculate rotation angle (0 is straight up / 12 o'clock)
    float rotAngle = 0.0f;
    if (knobStyle == KnobStyle::GainKnob)
    {
        // 0.0 dB is unity gain at 12 o'clock (rotAngle = 0)
        if (value >= 0.0)
        {
            float norm = static_cast<float>(value / (maxValue > 0.0 ? maxValue : 24.0));
            rotAngle = norm * maxAngle;
        }
        else
        {
            // 0 to -20 dB maps from 0 to -75% of travel (-101.25°)
            // -20 to -60 dB maps the remaining 25% down to -135°
            if (value >= -20.0)
            {
                float norm = static_cast<float>(value / -20.0);
                rotAngle = -norm * (maxAngle * 0.75f);
            }
            else
            {
                float norm = static_cast<float>((-value - 20.0) / 40.0);
                rotAngle = -(maxAngle * 0.75f + norm * (maxAngle * 0.25f));
            }
        }
    }
    else if (bipolar)
    {
        // -1..1 -> -135° .. +135°
        rotAngle = static_cast<float>(value) * maxAngle;
    }
    else
    {
        // 0..1 -> -135° .. +135°
        float norm = static_cast<float>(getNormalisedValue());
        rotAngle = -maxAngle + norm * (2.0f * maxAngle);
    }

    if (knobImg.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        juce::AffineTransform t = juce::AffineTransform::translation(-knobImg.getWidth() * 0.5f, -knobImg.getHeight() * 0.5f)
                                       .scaled(knobDiameter / static_cast<float>(knobImg.getWidth()),
                                               knobDiameter / static_cast<float>(knobImg.getHeight()))
                                       .rotated(rotAngle)
                                       .translated(centreX, centreY);

        // Must pass false so original image colors and 3D texture are drawn (not filled as mask)
        g.drawImageTransformed(knobImg, t, false);

        // Crisp white indicator notch overlay ensuring pointer is razor-sharp at any resolution
        g.setColour(juce::Colours::white);
        const float nStart = knobRadius * 0.42f;
        const float nEnd = knobRadius * 0.90f;
        const float sinA = std::sin(rotAngle);
        const float cosA = std::cos(rotAngle);
        g.drawLine(centreX + nStart * sinA, centreY - nStart * cosA,
                   centreX + nEnd * sinA,   centreY - nEnd * cosA,
                   1.8f);
    }
    else
    {
        // Fallback drawn circle
        g.setColour(juce::Colour(0xff24272E));
        g.fillEllipse(centreX - knobRadius, centreY - knobRadius, knobDiameter, knobDiameter);
        g.setColour(juce::Colours::white);
        g.drawLine(centreX, centreY, centreX + knobRadius * std::sin(rotAngle), centreY - knobRadius * std::cos(rotAngle), 2.0f);
    }
}

void DSDKnob::mouseDown(const juce::MouseEvent& e)
{
    dragStartValue = getNormalisedValue();
    dragStartPos = e.position;
}

void DSDKnob::mouseDrag(const juce::MouseEvent& e)
{
    float deltaY = dragStartPos.y - e.position.y;
    constexpr double sensitivity = 0.005;
    double newNorm = dragStartValue + static_cast<double>(deltaY) * sensitivity;
    setNormalisedValue(newNorm, true);
}

void DSDKnob::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    setValue(defaultValue, true);
}

void DSDKnob::mouseWheelMove(const juce::MouseEvent& /*e*/, const juce::MouseWheelDetails& w)
{
    double norm = getNormalisedValue();
    norm += 0.02 * static_cast<double>(w.deltaY > 0.0f ? 1.0 : -1.0);
    setNormalisedValue(norm, true);
}

} // namespace dsd
