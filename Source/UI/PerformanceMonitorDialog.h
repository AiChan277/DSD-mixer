#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioEngine.h"
#include <vector>

namespace dsd
{
    class PerformanceMonitorDialog : public juce::Component, public juce::Timer
    {
    public:
        PerformanceMonitorDialog(AudioEngine& engine);
        ~PerformanceMonitorDialog() override;

        void timerCallback() override;
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        AudioEngine& audioEngineRef;

        juce::Label titleLabel;
        juce::Label overallCpuLabel;
        juce::Label timeMetricsLabel;
        juce::Label xrunCounterLabel;
        juce::TextButton resetXrunBtn{"RESET XRUN"};

        struct WorkerRow
        {
            std::unique_ptr<juce::Label> nameLabel;
            std::unique_ptr<juce::ProgressBar> bar;
            double progressValue{0.0};
        };

        std::vector<WorkerRow> workerRows;
    };
} // namespace dsd
