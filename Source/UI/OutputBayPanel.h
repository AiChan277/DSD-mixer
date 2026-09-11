#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Output/OutputManager.h"
#include "UI/OutputBayStrip.h"
#include <vector>
#include <memory>

namespace dsd
{
    class OutputBayPanel : public juce::Component
    {
    public:
        OutputBayPanel(OutputManager& outManager);
        ~OutputBayPanel() override = default;

        void updateMeters();

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        OutputManager& outputManagerRef;

        juce::Label headerLabel;
        std::vector<std::unique_ptr<OutputBayStrip>> outputStrips;
    };
} // namespace dsd
