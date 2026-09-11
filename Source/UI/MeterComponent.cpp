#include "UI/MeterComponent.h"
#include "UI/DSDLookAndFeel.h"
#include "DSP/GainProcessor.h"

namespace dsd
{
    MeterComponent::MeterComponent(bool isStereoMode)
        : isStereo(isStereoMode)
    {
        setOpaque(false);
    }

    void MeterComponent::setMeterValues(float peakL, float peakR, float peakHoldL, float peakHoldR, bool clipped)
    {
        currentPeakL = peakL;
        currentPeakR = peakR;
        currentHoldL = peakHoldL;
        currentHoldR = peakHoldR;
        isClipped = clipped;
        repaint();
    }

    void MeterComponent::mouseDown(const juce::MouseEvent& event)
    {
        // Top clip LED area reset
        if (event.y < 16)
        {
            isClipped = false;
            if (onClipReset)
                onClipReset();
            repaint();
        }
    }

    float MeterComponent::linearToMeterProportion(float linear) noexcept
    {
        if (linear <= 0.001f) // <= -60 dBFS
            return 0.0f;
        
        const float db = GainProcessor::linearToDb(linear);
        // Map -60 dB .. 0 dB to 0.0 .. 1.0
        float prop = (db + 60.0f) / 60.0f;
        return std::clamp(prop, 0.0f, 1.0f);
    }

    void MeterComponent::drawSingleMeterBar(juce::Graphics& g, const juce::Rectangle<float>& bounds, float peak, float hold)
    {
        // Dark recessed meter background
        g.setColour(juce::Colour(0xff121316));
        g.fillRoundedRectangle(bounds, 2.0f);

        const float prop = linearToMeterProportion(peak);
        const float activeHeight = bounds.getHeight() * prop;
        const float barY = bounds.getBottom() - activeHeight;

        if (activeHeight > 0.5f)
        {
            juce::Rectangle<float> fillRect(bounds.getX(), barY, bounds.getWidth(), activeHeight);

            // Multistage LED broadcast gradient: Green (-60..-18) -> Yellow (-18..-6) -> Red (-6..0)
            juce::ColourGradient grad(DSDLookAndFeel::getMeterRed(), bounds.getX(), bounds.getY(),
                                      DSDLookAndFeel::getMeterGreen(), bounds.getX(), bounds.getBottom(), false);
            grad.addColour(0.30, DSDLookAndFeel::getMeterYellow());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, 1.5f);
        }

        // Peak Hold line
        const float holdProp = linearToMeterProportion(hold);
        if (holdProp > 0.02f)
        {
            const float holdY = bounds.getBottom() - (bounds.getHeight() * holdProp);
            g.setColour(holdProp > 0.95f ? DSDLookAndFeel::getMeterRed() : juce::Colours::white);
            g.fillRect(bounds.getX(), holdY - 1.0f, bounds.getWidth(), 2.0f);
        }
    }

    void MeterComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Clip LED circle at top
        const float clipSize = 8.0f;
        const float clipX = bounds.getCentreX() - clipSize * 0.5f;
        const float clipY = 2.0f;

        if (isClipped)
        {
            // Glowing red clip indicator
            g.setColour(DSDLookAndFeel::getMeterRed());
            g.fillEllipse(clipX, clipY, clipSize, clipSize);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawEllipse(clipX, clipY, clipSize, clipSize, 1.0f);
        }
        else
        {
            // Dim dark indicator
            g.setColour(juce::Colour(0xff3f1010));
            g.fillEllipse(clipX, clipY, clipSize, clipSize);
            g.setColour(juce::Colour(0xff2d2f36));
            g.drawEllipse(clipX, clipY, clipSize, clipSize, 0.8f);
        }

        // 2. Meter Bar Area
        auto meterArea = bounds.withTrimmedTop(14.0f).withTrimmedBottom(2.0f);

        if (isStereo)
        {
            const float barWidth = (meterArea.getWidth() - 3.0f) * 0.5f;
            drawSingleMeterBar(g, meterArea.withWidth(barWidth), currentPeakL, currentHoldL);
            drawSingleMeterBar(g, meterArea.withLeft(meterArea.getX() + barWidth + 3.0f), currentPeakR, currentHoldR);
        }
        else
        {
            drawSingleMeterBar(g, meterArea, currentPeakL, currentHoldL);
        }
    }
} // namespace dsd
