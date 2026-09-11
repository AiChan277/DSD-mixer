#include "UI/OutputBayPanel.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    OutputBayPanel::OutputBayPanel(OutputManager& outManager)
        : outputManagerRef(outManager)
    {
        headerLabel.setText("OUTPUT BAYS (4 CHANNELS)", juce::dontSendNotification);
        headerLabel.setJustificationType(juce::Justification::centred);
        headerLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        headerLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        headerLabel.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
        headerLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(headerLabel);

        const int numOutputs = outputManagerRef.getNumOutputs();
        for (int i = 0; i < numOutputs; ++i)
        {
            if (auto* out = outputManagerRef.getOutput(i))
            {
                auto strip = std::make_unique<OutputBayStrip>(*out);
                addAndMakeVisible(strip.get());
                outputStrips.push_back(std::move(strip));
            }
        }
    }

    void OutputBayPanel::updateMeters()
    {
        for (auto& strip : outputStrips)
        {
            if (strip != nullptr)
                strip->updateMeterFromAudio();
        }
    }

    void OutputBayPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(2, 2);
        headerLabel.setBounds(bounds.removeFromTop(22));
        bounds.removeFromTop(6);

        const int numStrips = static_cast<int>(outputStrips.size());
        if (numStrips <= 0) return;

        const int gap = 6;
        const int stripW = (bounds.getWidth() - (numStrips - 1) * gap) / numStrips;

        for (int i = 0; i < numStrips; ++i)
        {
            outputStrips[i]->setBounds(bounds.getX() + i * (stripW + gap), bounds.getY(), stripW, bounds.getHeight());
        }
    }

    void OutputBayPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff22242a));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }
} // namespace dsd
