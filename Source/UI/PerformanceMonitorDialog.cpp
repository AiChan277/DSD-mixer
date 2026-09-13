#include "UI/PerformanceMonitorDialog.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    PerformanceMonitorDialog::PerformanceMonitorDialog(AudioEngine& engine)
        : audioEngineRef(engine)
    {
        // ====================================================================
        // PANEL 1: Audio Engine Performance & Realtime Metrics
        // ====================================================================
        titleLabel.setText("DSD MIXER - MULTICORE PERFORMANCE MONITOR", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        processCpuLabel.setText("Process CPU: 0.0% [MEASURED: Win32 GetProcessTimes()]", juce::dontSendNotification);
        processCpuLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        processCpuLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(processCpuLabel);

        dspLoadLabel.setText("DSD DSP Load: 0.0% [Current]  |  0.0% [Avg 250ms]  |  0.0% [Peak]", juce::dontSendNotification);
        dspLoadLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        dspLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(dspLoadLabel);

        dspFormulaLabel.setText("DSP Load (%) = (DSP Processing Time / DSP Deadline) * 100", juce::dontSendNotification);
        dspFormulaLabel.setFont(juce::FontOptions(10.5f, juce::Font::italic));
        dspFormulaLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(dspFormulaLabel);

        timeMetricsLabel.setText("DSP Time: 0.00 ms [Current]  |  0.00 ms [Peak]  |  DSP Deadline: 10.00 ms", juce::dontSendNotification);
        timeMetricsLabel.setFont(juce::FontOptions(11.0f));
        timeMetricsLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(timeMetricsLabel);

        headroomLabel.setText("DSP Headroom: 10.00 ms [Current]  |  10.00 ms [Min Headroom]", juce::dontSendNotification);
        headroomLabel.setFont(juce::FontOptions(11.0f));
        headroomLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(headroomLabel);

        overrunStatusLabel.setText("Deadline Misses: 0  |  Device XRUNs: 0  [STATUS: REALTIME OK]", juce::dontSendNotification);
        overrunStatusLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        overrunStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(overrunStatusLabel);

        resetMetricsBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        resetMetricsBtn.onClick = [this]()
        {
            audioEngineRef.resetPerformanceMetrics();
            processCpuTracker.reset();
        };
        addAndMakeVisible(resetMetricsBtn);

        // ====================================================================
        // PANEL 2: Adaptive Multicore DSP Scheduler
        // ====================================================================
        schedulerSectionLabel.setText("ADAPTIVE MULTICORE DSP SCHEDULER: 4 / 8 ACTIVE", juce::dontSendNotification);
        schedulerSectionLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        schedulerSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(schedulerSectionLabel);

        schedulerPolicyLabel.setText("Policy: Fast Attack (>=4 blocks >60%), Slow Decay (>=300 blocks <30%)", juce::dontSendNotification);
        schedulerPolicyLabel.setFont(juce::FontOptions(10.5f));
        schedulerPolicyLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(schedulerPolicyLabel);

        const auto& sched = audioEngineRef.getScheduler();
        const int numWorkers = sched.getNumWorkers();

        for (int i = 0; i < numWorkers; ++i)
        {
            WorkerRow row;
            row.nameLabel = std::make_unique<juce::Label>();
            row.nameLabel->setText(juce::String::formatted("Worker %02d [ACTIVE]", i + 1), juce::dontSendNotification);
            row.nameLabel->setFont(juce::FontOptions(11.0f));
            row.nameLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
            addAndMakeVisible(row.nameLabel.get());

            row.bar = std::make_unique<juce::ProgressBar>(row.progressValue);
            row.bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getAccentBlue());
            row.bar->setColour(juce::ProgressBar::backgroundColourId, DSDLookAndFeel::getOledBlack());
            addAndMakeVisible(row.bar.get());

            row.detailLabel = std::make_unique<juce::Label>();
            row.detailLabel->setText("0.0% Util  (0 ch)", juce::dontSendNotification);
            row.detailLabel->setFont(juce::FontOptions(10.5f));
            row.detailLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
            addAndMakeVisible(row.detailLabel.get());

            workerRows.push_back(std::move(row));
        }

        // ====================================================================
        // PANEL 3: Signal Path & Latency Diagnostics
        // ====================================================================
        captureSectionLabel.setText("SIGNAL PATH & LATENCY DIAGNOSTICS", juce::dontSendNotification);
        captureSectionLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        captureSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(captureSectionLabel);

        latencyProfileLabel.setText("Capture Cushion Profile:", juce::dontSendNotification);
        latencyProfileLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        latencyProfileLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(latencyProfileLabel);

        auto setupLatencyBtn = [this](juce::TextButton& btn, CaptureLatencyMode mode)
        {
            btn.onClick = [this, mode]()
            {
                audioEngineRef.setCaptureLatencyMode(mode);
            };
            addAndMakeVisible(btn);
        };

        setupLatencyBtn(ultraLowLatencyBtn, CaptureLatencyMode::UltraLow);
        setupLatencyBtn(lowLatencyBtn, CaptureLatencyMode::Low);
        setupLatencyBtn(standardLatencyBtn, CaptureLatencyMode::Standard);

        profileHelpLabel.setText("Target Loopback Cushion: 2.7 ms (Ultra-Low) | 4.0 ms (Low) | 6.7 ms (Standard)", juce::dontSendNotification);
        profileHelpLabel.setFont(juce::FontOptions(10.5f));
        profileHelpLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(profileHelpLabel);

        sourceInfoLabel.setText("Capture Path: Direct Hardware / Sine (Synchronous Host Audio Clock)", juce::dontSendNotification);
        sourceInfoLabel.setFont(juce::FontOptions(11.0f));
        sourceInfoLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(sourceInfoLabel);

        pathBreakdownLabel1.setText("Capture Delivery: N/A | FIFO Cushion: 0 ms (Direct Pass-through)", juce::dontSendNotification);
        pathBreakdownLabel1.setFont(juce::FontOptions(10.5f));
        pathBreakdownLabel1.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(pathBreakdownLabel1);

        pathBreakdownLabel2.setText("DSP Processing: 0.00 ms [MEASURED] | Output Buffer: -- [CONFIGURED]", juce::dontSendNotification);
        pathBreakdownLabel2.setFont(juce::FontOptions(10.5f));
        pathBreakdownLabel2.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(pathBreakdownLabel2);

        totalLatencyLabel.setText("Estimated Path Latency: --", juce::dontSendNotification);
        totalLatencyLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        totalLatencyLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(totalLatencyLabel);

        startTimerHz(30);
        setSize(640, 700);
    }

    PerformanceMonitorDialog::~PerformanceMonitorDialog()
    {
        stopTimer();
    }

    void PerformanceMonitorDialog::timerCallback()
    {
        // 1. Process CPU (Win32 GetProcessTimes)
        const float procCpu = processCpuTracker.getCpuUsagePercent();
        processCpuLabel.setText(juce::String::formatted("Process CPU: %.1f%% [MEASURED: Win32 GetProcessTimes()]", procCpu),
                                juce::dontSendNotification);

        // 2. Audio Engine Realtime Telemetry
        const auto& stats = audioEngineRef.getPerformanceStats();
        const float currLoad = stats.currentLoadPercent.load(std::memory_order_relaxed);
        const float avgLoad = stats.avgLoadPercent.load(std::memory_order_relaxed);
        const float peakLoad = stats.peakLoadPercent.load(std::memory_order_relaxed);

        const double currProcMs = stats.currentProcessingTimeMs.load(std::memory_order_relaxed);
        const double peakProcMs = stats.peakProcessingTimeMs.load(std::memory_order_relaxed);
        const double deadMs = stats.deadlineMs.load(std::memory_order_relaxed);

        const double currHeadroom = stats.currentHeadroomMs.load(std::memory_order_relaxed);
        const double minHeadroom = stats.minHeadroomMs.load(std::memory_order_relaxed);

        const uint64_t deadlineMisses = stats.deadlineMissCount.load(std::memory_order_relaxed);
        const uint64_t xruns = stats.xrunCount.load(std::memory_order_relaxed);
        const bool isGlitching = stats.isGlitching.load(std::memory_order_relaxed);

        dspLoadLabel.setText(juce::String::formatted("DSD DSP Load: %.1f%% [Current]  |  %.1f%% [Avg 250ms]  |  %.1f%% [Peak]",
                                                     currLoad, avgLoad, peakLoad),
                             juce::dontSendNotification);

        if (currLoad > 75.0f || peakLoad > 85.0f)
            dspLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
        else if (currLoad > 40.0f || peakLoad > 60.0f)
            dspLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getMeterYellow());
        else
            dspLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());

        timeMetricsLabel.setText(juce::String::formatted("DSP Time: %.2f ms [Current]  |  %.2f ms [Peak]  |  DSP Deadline: %.2f ms (%d smp @ %.0f Hz)",
                                                         currProcMs, peakProcMs, deadMs,
                                                         audioEngineRef.getCurrentBlockSize(),
                                                         audioEngineRef.getCurrentSampleRate()),
                                 juce::dontSendNotification);

        headroomLabel.setText(juce::String::formatted("DSP Headroom: %.2f ms [Current]  |  %.2f ms [Min Headroom]",
                                                      currHeadroom, minHeadroom),
                              juce::dontSendNotification);

        if (deadlineMisses > 0 || xruns > 0 || isGlitching)
        {
            overrunStatusLabel.setText(juce::String::formatted("Deadline Misses: %llu  |  Device XRUNs: %llu  [STATUS: DEADLINE OVERRUN / DROPOUT]",
                                                               deadlineMisses, xruns),
                                       juce::dontSendNotification);
            overrunStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
        }
        else
        {
            overrunStatusLabel.setText("Deadline Misses: 0  |  Device XRUNs: 0  [STATUS: REALTIME OK]", juce::dontSendNotification);
            overrunStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        }

        // 3. Adaptive Worker Pool Telemetry
        const auto& sched = audioEngineRef.getScheduler();
        const int activeWorkers = audioEngineRef.getActiveWorkerCount();
        const int maxWorkers = audioEngineRef.getMaxWorkerCount();
        const float emaLoad = audioEngineRef.getSchedulerEmaLoad();
        const int activeTasks = audioEngineRef.getChannelManager().getNumChannels();
        const int graphFloor = (activeTasks >= 16) ? 6 : ((activeTasks >= 8) ? 4 : 2);

        schedulerSectionLabel.setText(juce::String::formatted(
            "ADAPTIVE DSP WORKER POOL: %d / %d ACTIVE  (Pool Load: %.1f%%, Floor: %d)",
            activeWorkers, maxWorkers, emaLoad, graphFloor),
            juce::dontSendNotification);

        for (int i = 0; i < static_cast<int>(workerRows.size()); ++i)
        {
            if (const auto* wStat = sched.getWorkerStats(i))
            {
                const bool isParked = wStat->isParked.load(std::memory_order_relaxed);
                if (isParked)
                {
                    workerRows[i].nameLabel->setText(juce::String::formatted("Worker %02d [PARKED]", i + 1), juce::dontSendNotification);
                    workerRows[i].nameLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary().withAlpha(0.45f));
                    workerRows[i].bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getTextSecondary().withAlpha(0.12f));
                    workerRows[i].progressValue = 0.0;
                    workerRows[i].detailLabel->setText("--  (Idle)", juce::dontSendNotification);
                    workerRows[i].detailLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary().withAlpha(0.45f));
                }
                else
                {
                    const float util = wStat->workerUtilizationPercent.load(std::memory_order_relaxed);
                    const int tasks = wStat->tasksAssigned.load(std::memory_order_relaxed);
                    const double busyMs = wStat->busyTimeMs.load(std::memory_order_relaxed);

                    workerRows[i].nameLabel->setText(juce::String::formatted("Worker %02d [ACTIVE]", i + 1), juce::dontSendNotification);
                    workerRows[i].nameLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());

                    if (util > 75.0f)
                        workerRows[i].bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getAccentRed());
                    else if (util > 40.0f)
                        workerRows[i].bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getMeterYellow());
                    else
                        workerRows[i].bar->setColour(juce::ProgressBar::foregroundColourId, DSDLookAndFeel::getAccentBlue());

                    workerRows[i].progressValue = std::clamp(static_cast<double>(util / 100.0), 0.0, 1.0);
                    workerRows[i].detailLabel->setText(juce::String::formatted("%.1f%% Util  (%d ch, %.2f ms)", util, tasks, busyMs),
                                                       juce::dontSendNotification);
                    workerRows[i].detailLabel->setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
                }
            }
        }

        // 4. Latency Profile & Signal Path Diagnostics
        const auto activeMode = audioEngineRef.getCaptureLatencyMode();
        auto updateBtnStyle = [activeMode](juce::TextButton& btn, CaptureLatencyMode mode)
        {
            if (activeMode == mode)
            {
                btn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentAmber());
                btn.setColour(juce::TextButton::textColourOnId, DSDLookAndFeel::getConsoleDarkBg());
                btn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getConsoleDarkBg());
            }
            else
            {
                btn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
                btn.setColour(juce::TextButton::textColourOnId, DSDLookAndFeel::getTextPrimary());
                btn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextSecondary());
            }
        };

        updateBtnStyle(ultraLowLatencyBtn, CaptureLatencyMode::UltraLow);
        updateBtnStyle(lowLatencyBtn, CaptureLatencyMode::Low);
        updateBtnStyle(standardLatencyBtn, CaptureLatencyMode::Standard);

        auto diags = audioEngineRef.getActiveCaptureDiagnostics();
        const double dspExecMs = audioEngineRef.getDspExecutionTimeMs();
        const int outBlockFrames = audioEngineRef.getCurrentBlockSize();
        const double outBufferMs = audioEngineRef.getEstimatedOutputLatencyMs();

        if (!diags.empty())
        {
            const auto& d = diags.front();
            sourceInfoLabel.setText(juce::String::formatted("Capture Path: Windows Process Loopback (%s, PID: %u) | %.0f Hz, %s %d-bit %s",
                                                           d.appName.toRawUTF8(), d.processId, d.captureSampleRate,
                                                           d.isFloat ? "Float" : "PCM", d.bitsPerSample,
                                                           (d.captureChannels >= 2 ? "Stereo" : "Mono")),
                                    juce::dontSendNotification);

            const double hwAgeMs = (d.hardwarePacketAgeMs > 0.1) ? d.hardwarePacketAgeMs : d.wasapiBufferMs;

            pathBreakdownLabel1.setText(juce::String::formatted("Capture Delivery: %.1f ms [MEASURED: WASAPI QPC Packet Age]  |  FIFO Cushion: %.1f ms (%d / %d fr) [MEASURED]",
                                                                hwAgeMs, d.ringBufferOccupancyMs, d.ringBufferOccupancy, d.targetOccupancy),
                                        juce::dontSendNotification);

            pathBreakdownLabel2.setText(juce::String::formatted("DSP Processing: %.2f ms [MEASURED: CPU Execution]  |  Output Buffer: %d frames / %.2f ms [CONFIGURED]",
                                                                dspExecMs, outBlockFrames, outBufferMs),
                                        juce::dontSendNotification);

            const double totalMeasured = hwAgeMs + d.ringBufferOccupancyMs + dspExecMs + outBufferMs;
            totalLatencyLabel.setText(juce::String::formatted("Total Path Latency: ~%.1f ms [ESTIMATED: Measured Delivery + Configured Output]  |  Drift: %+.1f PPM [MEASURED]",
                                                              totalMeasured, d.clockDriftPpm),
                                      juce::dontSendNotification);
        }
        else
        {
            sourceInfoLabel.setText("Capture Path: Direct Hardware / Sine Generator (Synchronous ADC / Host Clock)", juce::dontSendNotification);

            pathBreakdownLabel1.setText("Capture Delivery: N/A (Direct Hardware)  |  FIFO Cushion: 0 ms (Direct Pass-through)",
                                        juce::dontSendNotification);

            pathBreakdownLabel2.setText(juce::String::formatted("DSP Processing: %.2f ms [MEASURED: CPU Execution]  |  Output Buffer: %d frames / %.2f ms [CONFIGURED: AudioDeviceManager]",
                                                                dspExecMs, outBlockFrames, outBufferMs),
                                        juce::dontSendNotification);

            const double totalEst = dspExecMs + outBufferMs;
            totalLatencyLabel.setText(juce::String::formatted("Estimated Path Latency: ~%.2f ms [ESTIMATED: DSP Processing + Output Buffer]  |  Drift: Not measured",
                                                              totalEst),
                                      juce::dontSendNotification);
        }
    }

    void PerformanceMonitorDialog::resized()
    {
        auto bounds = getLocalBounds().reduced(16, 14);

        // PANEL 1: Audio Performance & Realtime Metrics
        titleLabel.setBounds(bounds.removeFromTop(22));
        bounds.removeFromTop(6);

        processCpuLabel.setBounds(bounds.removeFromTop(18));
        dspLoadLabel.setBounds(bounds.removeFromTop(18));
        dspFormulaLabel.setBounds(bounds.removeFromTop(15));
        bounds.removeFromTop(4);

        timeMetricsLabel.setBounds(bounds.removeFromTop(18));
        headroomLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(4);

        auto overrunRow = bounds.removeFromTop(24);
        overrunStatusLabel.setBounds(overrunRow.removeFromLeft(420));
        resetMetricsBtn.setBounds(overrunRow.removeFromRight(170));

        bounds.removeFromTop(14);

        // PANEL 2: Adaptive Multicore DSP Scheduler
        schedulerSectionLabel.setBounds(bounds.removeFromTop(20));
        schedulerPolicyLabel.setBounds(bounds.removeFromTop(15));
        bounds.removeFromTop(6);

        const int rowH = 18;
        const int gap = 4;
        for (auto& row : workerRows)
        {
            auto r = bounds.removeFromTop(rowH);
            row.nameLabel->setBounds(r.removeFromLeft(125));
            row.detailLabel->setBounds(r.removeFromRight(150));
            r.removeFromRight(8);
            row.bar->setBounds(r);
            bounds.removeFromTop(gap);
        }

        bounds.removeFromTop(12);

        // PANEL 3: Signal Path & Latency Diagnostics
        captureSectionLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(4);

        auto profileRow = bounds.removeFromTop(22);
        latencyProfileLabel.setBounds(profileRow.removeFromLeft(150));
        const int btnW = 100;
        ultraLowLatencyBtn.setBounds(profileRow.removeFromLeft(btnW));
        profileRow.removeFromLeft(6);
        lowLatencyBtn.setBounds(profileRow.removeFromLeft(btnW));
        profileRow.removeFromLeft(6);
        standardLatencyBtn.setBounds(profileRow.removeFromLeft(btnW));

        profileHelpLabel.setBounds(bounds.removeFromTop(15));
        bounds.removeFromTop(4);

        sourceInfoLabel.setBounds(bounds.removeFromTop(18));
        pathBreakdownLabel1.setBounds(bounds.removeFromTop(16));
        pathBreakdownLabel2.setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(4);
        totalLatencyLabel.setBounds(bounds.removeFromTop(22));
    }

    void PerformanceMonitorDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());

        // Decorative subtle dividers separating the 3 technical panels
        g.setColour(DSDLookAndFeel::getConsoleBevel().withAlpha(0.6f));
        g.drawHorizontalLine(162, 16.0f, static_cast<float>(getWidth() - 16));
        g.drawHorizontalLine(388, 16.0f, static_cast<float>(getWidth() - 16));
    }
} // namespace dsd
