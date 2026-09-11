#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel/ChannelManager.h"
#include "Output/OutputManager.h"
#include "Scheduler/DSPScheduler.h"

namespace dsd
{
    class StageInspectorDialog : public juce::Component, public juce::Timer
    {
    public:
        StageInspectorDialog(ChannelManager& chanMgr, OutputManager& outMgr, DSPScheduler& scheduler);
        ~StageInspectorDialog() override;

        void timerCallback() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        ChannelManager& channelManagerRef;
        OutputManager& outputManagerRef;
        DSPScheduler& schedulerRef;

        juce::ComboBox channelSelector;
        juce::TextButton dspModeBtn;
        juce::TextButton resetClipsBtn{"RESET CLIPS"};

        struct StageCard
        {
            juce::Label titleLabel;
            juce::Label peakLabel;
            juce::Label rmsLabel;
            juce::Label statusLabel;
        };

        StageCard stageA; // Input
        StageCard stageB; // Post-Gain
        StageCard stageC; // Post-VST
        StageCard stageD; // Post-Fader
        StageCard stageE; // Output Bus

        void setupCard(StageCard& card, const juce::String& title);
        void updateCard(StageCard& card, const MeterValues& mv, const juce::String& extra = {});
    };
} // namespace dsd
