#include "UI/MeterComponent.h"
#include "UI/DSDLookAndFeel.h"
#include "DSP/GainProcessor.h"

namespace dsd
{
    MeterComponent::MeterComponent(bool isStereoMode, bool isHorizontalMode)
        : isStereo(isStereoMode),
          isHorizontal(isHorizontalMode)
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
        if (event.y < 24 || isHorizontal)
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
        if (db <= -60.0f) return 0.0f;
        if (db <= -40.0f) return 0.00f + ((db + 60.0f) / 20.0f) * 0.17f;
        if (db <= -20.0f) return 0.17f + ((db + 40.0f) / 20.0f) * 0.17f;
        if (db <= -10.0f) return 0.34f + ((db + 20.0f) / 10.0f) * 0.17f;
        if (db <=  -6.0f) return 0.51f + ((db + 10.0f) /  4.0f) * 0.17f;
        if (db <=  -3.0f) return 0.68f + ((db +  6.0f) /  3.0f) * 0.16f;
        if (db <=   0.0f) return 0.84f + ((db +  3.0f) /  3.0f) * 0.16f;
        return 1.0f;
    }

    void MeterComponent::drawVerticalBar(juce::Graphics& g, const juce::Rectangle<float>& bounds, float peak, float hold)
    {
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(bounds, 2.0f);

        const float prop = linearToMeterProportion(peak);
        const float activeHeight = bounds.getHeight() * prop;
        const float barY = bounds.getBottom() - activeHeight;

        if (activeHeight > 0.5f)
        {
            juce::Rectangle<float> fillRect(bounds.getX(), barY, bounds.getWidth(), activeHeight);

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

    void MeterComponent::drawHorizontalSegmentedBar(juce::Graphics& g, const juce::Rectangle<float>& bounds, float peak, float hold)
    {
        const int numSegs = 26;
        const float gap = 1.0f;
        const float segHeight = 6.5f;
        const float segY = bounds.getY();

        const float totalW = bounds.getWidth();
        const float segW = std::max(1.0f, (totalW - (numSegs - 1) * gap) / static_cast<float>(numSegs));

        const float peakProp = linearToMeterProportion(peak);
        const float holdProp = linearToMeterProportion(hold);

        for (int i = 0; i < numSegs; ++i)
        {
            const float segProp = static_cast<float>(i + 1) / static_cast<float>(numSegs);

            juce::Colour litColour;
            juce::Colour unlitColour;

            if (segProp <= 0.51f) // Up to -10 dBFS
            {
                litColour = juce::Colour(0xff00E676);    // Broadcast LED green
                unlitColour = juce::Colour(0xff0A2210);  // Dim dark green
            }
            else if (segProp <= 0.84f) // -10 dBFS to -3 dBFS
            {
                litColour = juce::Colour(0xffFFD600);    // Broadcast LED amber/yellow
                unlitColour = juce::Colour(0xff242004);  // Dim dark yellow
            }
            else // -3 dBFS to 0 dBFS
            {
                litColour = juce::Colour(0xffFF1744);    // Broadcast LED red
                unlitColour = juce::Colour(0xff2B090D);  // Dim dark red
            }

            const bool isLit = (segProp <= peakProp + (0.5f / numSegs));
            const bool isHold = (std::abs(segProp - holdProp) < (0.6f / numSegs)) && (holdProp > 0.05f);

            const float x = bounds.getX() + i * (segW + gap);
            auto segRect = juce::Rectangle<float>(x, segY, segW, segHeight);

            if (isLit)
            {
                g.setColour(litColour);
                g.fillRoundedRectangle(segRect, 0.5f);
            }
            else if (isHold)
            {
                g.setColour(juce::Colours::white);
                g.fillRoundedRectangle(segRect, 0.5f);
            }
            else
            {
                g.setColour(unlitColour);
                g.fillRoundedRectangle(segRect, 0.5f);
                g.setColour(juce::Colour(0xff1A1D24));
                g.drawRoundedRectangle(segRect, 0.5f, 0.5f);
            }
        }

        // Scale markings underneath the segmented bar
        // Spaced evenly across width with non-overlapping bounds
        const float textY = segY + segHeight + 1.5f;
        g.setFont(juce::FontOptions(6.5f));
        g.setColour(juce::Colour(0xff757B88));

        const struct { float normPos; const char* txt; } scale[] = {
            { 0.05f, "-60" },
            { 0.20f, "-40" },
            { 0.38f, "-20" },
            { 0.55f, "-10" },
            { 0.71f,  "-6" },
            { 0.86f,  "-3" },
            { 0.96f,   "0" }
        };

        for (const auto& s : scale)
        {
            const float markerX = bounds.getX() + s.normPos * totalW;
            const float w = (s.txt[0] == '-' && s.txt[2] != '\0') ? 13.0f : ((s.txt[0] == '-') ? 10.0f : 7.0f);
            g.drawText(s.txt, static_cast<int>(markerX - w * 0.5f), static_cast<int>(textY),
                       static_cast<int>(w), 10, juce::Justification::centred, false);
        }
    }

    void MeterComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        if (isHorizontal)
        {
            float peak = isStereo ? std::max(currentPeakL, currentPeakR) : currentPeakL;
            float hold = isStereo ? std::max(currentHoldL, currentHoldR) : currentHoldL;
            drawHorizontalSegmentedBar(g, bounds, peak, hold);
            return;
        }

        // 1. Clip LED circle at top center (for vertical mode)
        const float clipSize = 7.0f;
        const float clipX = bounds.getCentreX() - clipSize * 0.5f;
        const float clipY = 2.0f;

        if (isClipped)
        {
            g.setColour(DSDLookAndFeel::getMeterRed());
            g.fillEllipse(clipX, clipY, clipSize, clipSize);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawEllipse(clipX, clipY, clipSize, clipSize, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff3f1010));
            g.fillEllipse(clipX, clipY, clipSize, clipSize);
            g.setColour(DSDLookAndFeel::getConsoleBevel());
            g.drawEllipse(clipX, clipY, clipSize, clipSize, 0.8f);
        }

        if (isStereo)
        {
            const float barGap = 4.0f;
            const float barWidth = (bounds.getWidth() - barGap) * 0.5f;
            const float leftBarX = bounds.getX();
            const float rightBarX = bounds.getX() + barWidth + barGap;

            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.setColour(juce::Colour(0xffADB2BA));
            g.drawText("L", static_cast<int>(leftBarX), 11, static_cast<int>(barWidth), 12, juce::Justification::centred, false);
            g.drawText("R", static_cast<int>(rightBarX), 11, static_cast<int>(barWidth), 12, juce::Justification::centred, false);

            auto barsArea = bounds.withTrimmedTop(24.0f).withTrimmedBottom(2.0f);
            drawVerticalBar(g, juce::Rectangle<float>(leftBarX, barsArea.getY(), barWidth, barsArea.getHeight()), currentPeakL, currentHoldL);
            drawVerticalBar(g, juce::Rectangle<float>(rightBarX, barsArea.getY(), barWidth, barsArea.getHeight()), currentPeakR, currentHoldR);
        }
        else
        {
            auto meterArea = bounds.withTrimmedTop(14.0f).withTrimmedBottom(2.0f);
            drawVerticalBar(g, meterArea, currentPeakL, currentHoldL);
        }
    }
} // namespace dsd
