#include "UI/PerformanceMonitorDialog.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    PerformanceMonitorDialog::PerformanceMonitorDialog(AudioEngine& engine)
        : audioEngineRef(engine)
    {
        titleLabel.setText("DSD MIXER - MULTICORE DSP PERFORMANCE MONITOR", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        overallCpuLabel.setText("Overall Audio Load: 0.0%", juce::dontSendNotification);
        overallCpuLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        overallCpuLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(overallCpuLabel);

        timeMetricsLabel.setText("Deadline: 2.67 ms | DSP Time: 0.00 ms | Headroom: 2.67 ms", juce::dontSendNotification);
        timeMetricsLabel.setFont(juce::FontOptions(11.0f));
        timeMetricsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(timeMetricsLabel);

        xrunCounterLabel.setText("XRUN / Dropouts: 0 (STATUS: AUDIO OK)", juce::dontSendNotification);
        xrunCounterLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        xrunCounterLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(xrunCounterLabel);

        resetXrunBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        resetXrunBtn.onClick = [this]()
        {
            audioEngineRef.getPerformanceStats().xrunCount.store(0);
            audioEngineRef.getPerformanceStats().isGlitching.store(false);
        };
        addAndMakeVisible(resetXrunBtn);

        // Workers breakdown
        const auto& sched = audioEngineRef.getScheduler();
        const int numWorkers = sched.getNumWorkers();

        for (int i = 0; i < numWorkers; ++i)
        {
            WorkerRow row;
            row.nameLabel = std::make_unique<juce::Label>();
            row.nameLabel->setText(juce::String::formatted("DSP Worker %02d", i + 1), juce::dontSendNotification);
            row.nameLabel->setFont(juce::FontOptions(11.0f));
            row.nameLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
            addAndMakeVisible(row.nameLabel.get());

            row.bar = std::make_unique<juce::ProgressBar>(row.progressValue);
            row.bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getAccentBlue());
            row.bar->setColour(juce::ProgressBar::backgroundColourId, DSDLookAndFeel::getOledBlack());
            addAndMakeVisible(row.bar.get());

            workerRows.push_back(std::move(row));
        }

        startTimerHz(30);
        setSize(540, 420);
    }

    PerformanceMonitorDialog::~PerformanceMonitorDialog()
    {
        stopTimer();
    }

    void PerformanceMonitorDialog::timerCallback()
    {
        const auto& stats = audioEngineRef.getPerformanceStats();
        const float cpu = stats.cpuLoadPercent.load(std::memory_order_relaxed);
        const double procMs = stats.processingTimeMs.load(std::memory_order_relaxed);
        const double deadMs = stats.deadlineMs.load(std::memory_order_relaxed);
        const double headroomMs = std::max(0.0, deadMs - procMs);
        const uint64_t xruns = stats.xrunCount.load(std::memory_order_relaxed);
        const bool glitch = stats.isGlitching.load(std::memory_order_relaxed);

        overallCpuLabel.setText(juce::String::formatted("Overall Audio Load: %.1f %%", cpu), juce::dontSendNotification);
        if (cpu > 75.0f)
            overallCpuLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
        else if (cpu > 40.0f)
            overallCpuLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
        else
            overallCpuLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());

        timeMetricsLabel.setText(juce::String::formatted("Deadline: %.2f ms | DSP Time: %.2f ms | Headroom: %.2f ms",
                                                        deadMs, procMs, headroomMs),
                                 juce::dontSendNotification);

        if (glitch || xruns > 0)
        {
            xrunCounterLabel.setText(juce::String::formatted("XRUN / Dropouts: %llu (STATUS: AUDIO GLITCH)", xruns),
                                    juce::dontSendNotification);
            xrunCounterLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
        }
        else
        {
            xrunCounterLabel.setText("XRUN / Dropouts: 0 (STATUS: AUDIO OK)", juce::dontSendNotification);
            xrunCounterLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        }

        // Update Workers
        const auto& sched = audioEngineRef.getScheduler();
        for (int i = 0; i < static_cast<int>(workerRows.size()); ++i)
        {
            if (const auto* wStat = sched.getWorkerStats(i))
            {
                float load = wStat->cpuLoadPercent.load(std::memory_order_relaxed);
                workerRows[i].progressValue = std::clamp(static_cast<double>(load / 100.0), 0.0, 1.0);
            }
        }
    }

    void PerformanceMonitorDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(16, 14);

        titleLabel.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(8);

        overallCpuLabel.setBounds(bounds.removeFromTop(20));
        timeMetricsLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(6);

        auto xrunRow = bounds.removeFromTop(26);
        xrunCounterLabel.setBounds(xrunRow.removeFromLeft(360));
        resetXrunBtn.setBounds(xrunRow.removeFromRight(100));

        bounds.removeFromTop(16);

        const int rowH = 22;
        const int gap = 6;
        for (auto& row : workerRows)
        {
            auto r = bounds.removeFromTop(rowH);
            row.nameLabel->setBounds(r.removeFromLeft(120));
            row.bar->setBounds(r);
            bounds.removeFromTop(gap);
        }
    }

    void PerformanceMonitorDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }
} // namespace dsd
