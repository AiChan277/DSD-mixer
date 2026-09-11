#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Routing/RoutingEngine.h"
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"

namespace dsd
{
    class RoutingMatrixDialog : public juce::Component
    {
    public:
        RoutingMatrixDialog(RoutingEngine& router, ChannelManager& chMgr, OutputManager& outMgr);
        ~RoutingMatrixDialog() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        RoutingEngine& routingEngineRef;
        ChannelManager& channelManagerRef;
        OutputManager& outputManagerRef;

        juce::Label titleLabel;

        struct MatrixCellComponent : public juce::Component
        {
            juce::ToggleButton enableBtn;
            juce::Slider       sendGainSlider;
            int chIdx{0};
            int outIdx{0};
        };

        std::vector<std::unique_ptr<MatrixCellComponent>> cells;
        std::vector<std::unique_ptr<juce::Label>> channelRowLabels;
        std::vector<std::unique_ptr<juce::Label>> outputColLabels;

        juce::Viewport viewport;
        juce::Component gridContainer;
    };
} // namespace dsd
