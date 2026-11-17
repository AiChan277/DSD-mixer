#include "UI/PerformanceMonitorDialog.h"
#include "UI/DSDLookAndFeel.h"

namespace dsd
{
    void BlockTimingHistogramView::setSnapshot(const BlockTimingHistogram::Snapshot& newSnapshot)
    {
        if (newSnapshot.total == snapshot.total && newSnapshot.counts == snapshot.counts)
            return;

        snapshot = newSnapshot;
        repaint();
    }

    void BlockTimingHistogramView::paint(juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();
        g.setColour(DSDLookAndFeel::getOledBlack());
        g.fillRoundedRectangle(area, 3.0f);

        const float labelH = 14.0f;
        auto plot = area.reduced(6.0f, 4.0f);
        auto labels = plot.removeFromBottom(labelH);

        uint64_t maxCount = 0;
        for (auto c : snapshot.counts)
            maxCount = std::max(maxCount, c);
        const double logMax = std::log10(static_cast<double>(maxCount) + 1.0);

        const int n = BlockTimingHistogram::kNumBuckets;
        const float slotW = plot.getWidth() / static_cast<float>(n);

        g.setFont(juce::FontOptions(9.5f));
        for (int i = 0; i < n; ++i)
        {
            const auto count = snapshot.counts[static_cast<size_t>(i)];
            const float x = plot.getX() + slotW * static_cast<float>(i);

            if (count > 0 && logMax > 0.0)
            {
                const float frac = static_cast<float>(std::log10(static_cast<double>(count) + 1.0) / logMax);
                const float barH = std::max(2.0f, plot.getHeight() * frac);
                juce::Colour colour = DSDLookAndFeel::getAccentGreen();
                if (i >= n - 1)
                    colour = DSDLookAndFeel::getAccentRed();
                else if (i >= 6)
                    colour = DSDLookAndFeel::getMeterYellow();
                g.setColour(colour);
                g.fillRect(x + 2.0f, plot.getBottom() - barH, slotW - 4.0f, barH);
            }

            const juce::String label = (i < n - 1)
                ? juce::String(juce::roundToInt(BlockTimingHistogram::kUpperEdges[static_cast<size_t>(i)] * 100.0f)) + "%"
                : juce::String(">100%");
            g.setColour(DSDLookAndFeel::getTextSecondary());
            g.drawText(label, juce::Rectangle<float>(x, labels.getY(), slotW, labelH), juce::Justification::centred);
        }
    }

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
        schedulerSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentBlue());
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
        captureSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9A3412));
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

        bridgeStatusLabel.setFont(juce::FontOptions(10.5f));
        bridgeStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        bridgeStatusLabel.setText("Secondary devices: none", juce::dontSendNotification);
        addAndMakeVisible(bridgeStatusLabel);

        pdcToggle.setColour(juce::ToggleButton::textColourId, DSDLookAndFeel::getTextPrimary());
        pdcToggle.setColour(juce::ToggleButton::tickColourId, DSDLookAndFeel::getAccentGreen());
        pdcToggle.setToggleState(audioEngineRef.isPluginDelayCompensationEnabled(), juce::dontSendNotification);
        pdcToggle.onClick = [this]()
        {
            audioEngineRef.setPluginDelayCompensationEnabled(pdcToggle.getToggleState());
        };
        addAndMakeVisible(pdcToggle);

        pdcInfoLabel.setFont(juce::FontOptions(10.5f));
        pdcInfoLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(pdcInfoLabel);

        // ====================================================================
        // PANEL 4: Block Timing Distribution
        // ====================================================================
        timingSectionLabel.setText("BLOCK TIMING DISTRIBUTION (DSP time as % of deadline)", juce::dontSendNotification);
        timingSectionLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        timingSectionLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentBlue());
        addAndMakeVisible(timingSectionLabel);

        timingSummaryLabel.setText("p95: --  |  p99: --  |  Blocks: 0  |  Over deadline: 0", juce::dontSendNotification);
        timingSummaryLabel.setFont(juce::FontOptions(11.0f));
        timingSummaryLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(timingSummaryLabel);

        addAndMakeVisible(timingHistogramView);

        timerCallback();
        startTimerHz(30);
        setSize(640, 900);
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

        // 4. Block timing distribution
        {
            const auto hist = audioEngineRef.getBlockTimingHistogram().snapshot();
            timingHistogramView.setSnapshot(hist);

            auto describe = [](float edge) -> juce::String
            {
                if (edge <= 0.0f)
                    return "--";
                if (edge > 1.0f)
                    return "> 100%";
                return "<= " + juce::String(juce::roundToInt(edge * 100.0f)) + "%";
            };

            const float p95 = hist.percentileUpperEdge(95.0);
            const float p99 = hist.percentileUpperEdge(99.0);
            const uint64_t over = hist.counts[static_cast<size_t>(BlockTimingHistogram::kNumBuckets - 1)];

            timingSummaryLabel.setText("p95: " + describe(p95) + "  |  p99: " + describe(p99)
                                       + "  |  Blocks: " + juce::String(static_cast<juce::int64>(hist.total))
                                       + "  |  Over deadline: " + juce::String(static_cast<juce::int64>(over)),
                                       juce::dontSendNotification);
            timingSummaryLabel.setColour(juce::Label::textColourId,
                                         over > 0 ? DSDLookAndFeel::getAccentRed()
                                                  : (p99 > 0.6f ? DSDLookAndFeel::getMeterYellow()
                                                                : DSDLookAndFeel::getTextSecondary()));
        }

        // 5. Secondary device bridges (drift-corrected)
        {
            const auto bridges = MultiDeviceManager::getInstance().getBridgeStatus();
            if (bridges.empty())
            {
                bridgeStatusLabel.setText("Secondary devices: none (single-clock path)", juce::dontSendNotification);
                bridgeStatusLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
            }
            else
            {
                juce::StringArray parts;
                bool problem = false;
                for (const auto& b : bridges)
                {
                    problem = problem || !b.streaming;
                    parts.add(juce::String(b.isInput ? "IN " : "OUT ") + b.deviceName.substring(0, 22)
                              + (b.streaming ? juce::String::formatted(" %+.0f ppm %.1f ms", b.driftPpm, b.bufferedMs)
                                             : juce::String(" RECONNECTING"))
                              + (b.underruns > 0 ? " u" + juce::String(static_cast<juce::int64>(b.underruns)) : juce::String())
                              + (b.reconnects > 0 ? " r" + juce::String(static_cast<juce::int64>(b.reconnects)) : juce::String()));
                }
                bridgeStatusLabel.setText("Secondary devices: " + parts.joinIntoString("  |  "), juce::dontSendNotification);
                bridgeStatusLabel.setColour(juce::Label::textColourId,
                                            problem ? DSDLookAndFeel::getAccentRed() : DSDLookAndFeel::getTextSecondary());
            }
        }

        // 5b. Plugin delay compensation
        {
            pdcToggle.setToggleState(audioEngineRef.isPluginDelayCompensationEnabled(), juce::dontSendNotification);
            const int pdcSamples = audioEngineRef.getCompensatedLatencySamples();
            const double sr = audioEngineRef.getCurrentSampleRate();
            const double pdcMs = (sr > 0.0) ? pdcSamples * 1000.0 / sr : 0.0;
            if (audioEngineRef.isPluginDelayCompensationEnabled())
                pdcInfoLabel.setText(juce::String::formatted("ON: all channels aligned to the slowest plugin chain (+%d smp / %.2f ms)",
                                                             pdcSamples, pdcMs),
                                     juce::dontSendNotification);
            else
                pdcInfoLabel.setText("OFF: channels with latency plugins arrive late at shared outputs (no added latency)",
                                     juce::dontSendNotification);
        }

        // 6. Latency Profile & Signal Path Diagnostics
        const auto activeMode = audioEngineRef.getCaptureLatencyMode();
        auto updateBtnStyle = [activeMode](juce::TextButton& btn, CaptureLatencyMode mode)
        {
            btn.setToggleState(activeMode == mode, juce::dontSendNotification);
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
            juce::String srcText = "Capture Path: Windows Process Loopback (" + d.appName
                                 + ", PID: " + juce::String(d.processId) + ") | "
                                 + juce::String(d.captureSampleRate, 0) + " Hz, "
                                 + (d.isFloat ? "Float " : "PCM ") + juce::String(d.bitsPerSample) + "-bit "
                                 + (d.captureChannels >= 2 ? "Stereo" : "Mono");
            sourceInfoLabel.setText(srcText, juce::dontSendNotification);

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
        bridgeStatusLabel.setBounds(bounds.removeFromTop(18));
        auto pdcRow = bounds.removeFromTop(24);
        pdcToggle.setBounds(pdcRow.removeFromLeft(210));
        pdcInfoLabel.setBounds(pdcRow);

        // PANEL 4: Block Timing Distribution
        bounds.removeFromTop(10);
        timingPanelDividerY = bounds.getY();
        bounds.removeFromTop(8);
        timingSectionLabel.setBounds(bounds.removeFromTop(20));
        timingSummaryLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(4);
        timingHistogramView.setBounds(bounds.removeFromTop(90));
    }

    void PerformanceMonitorDialog::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());

        // Decorative subtle dividers separating the 3 technical panels
        g.setColour(DSDLookAndFeel::getConsoleBevel().withAlpha(0.6f));
        g.drawHorizontalLine(162, 16.0f, static_cast<float>(getWidth() - 16));
        g.drawHorizontalLine(388, 16.0f, static_cast<float>(getWidth() - 16));
        if (timingPanelDividerY > 0)
            g.drawHorizontalLine(timingPanelDividerY, 16.0f, static_cast<float>(getWidth() - 16));
    }
} // namespace dsd
