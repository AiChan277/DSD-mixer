#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Audio/AudioEngine.h"
#include "Audio/ProcessCpuTracker.h"
#include <vector>

namespace dsd
{
    // Bar chart of how many audio blocks fell into each load band
    // (processing time as % of the block deadline). Bar height is log-scaled
    // so a handful of slow blocks stays visible next to millions of fast ones.
    class BlockTimingHistogramView : public juce::Component
    {
    public:
        void setSnapshot(const BlockTimingHistogram::Snapshot& newSnapshot);
        void paint(juce::Graphics& g) override;

    private:
        BlockTimingHistogram::Snapshot snapshot;
    };

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
        ProcessCpuTracker processCpuTracker;

        // Panel 1: Audio Engine Performance & Realtime Metrics
        juce::Label titleLabel;
        juce::Label processCpuLabel;
        juce::Label dspLoadLabel;
        juce::Label dspFormulaLabel;
        juce::Label timeMetricsLabel;
        juce::Label headroomLabel;
        juce::Label overrunStatusLabel;
        juce::TextButton resetMetricsBtn{"RESET PEAKS & METRICS"};

        // Panel 2: Adaptive Multicore DSP Scheduler
        juce::Label schedulerSectionLabel;
        juce::Label schedulerPolicyLabel;

        struct WorkerRow
        {
            std::unique_ptr<juce::Label> nameLabel;
            std::unique_ptr<juce::ProgressBar> bar;
            std::unique_ptr<juce::Label> detailLabel;
            double progressValue{0.0};
        };

        std::vector<WorkerRow> workerRows;

        // Panel 3: Signal Path & Latency Diagnostics
        juce::Label captureSectionLabel;
        juce::Label latencyProfileLabel;
        juce::TextButton ultraLowLatencyBtn{"ULTRA-LOW"};
        juce::TextButton lowLatencyBtn{"LOW"};
        juce::TextButton standardLatencyBtn{"STANDARD"};
        juce::Label profileHelpLabel;

        juce::Label sourceInfoLabel;
        juce::Label pathBreakdownLabel1;
        juce::Label pathBreakdownLabel2;
        juce::Label totalLatencyLabel;

        // Panel 4: Block Timing Distribution
        juce::Label timingSectionLabel;
        juce::Label timingSummaryLabel;
        BlockTimingHistogramView timingHistogramView;
        int timingPanelDividerY{0};
    };
} // namespace dsd
