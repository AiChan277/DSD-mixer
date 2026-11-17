#include "UI/TopBar.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/RoutingMatrixDialog.h"
#include "UI/PerformanceMonitorDialog.h"
#include "UI/StageInspectorDialog.h"
#include "UI/ControlServerDialog.h"
#include "UI/ToolsDialogs.h"
#include "Control/ControlServer.h"
#include "Session/SessionManager.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace dsd
{
    TopBar::TopBar(AudioDeviceManager& devManager, AudioEngine& audioEngine, control::ControlServer* controlServer)
        : deviceManagerRef(devManager), audioEngineRef(audioEngine), controlServerRef(controlServer)
    {
        titleLabel.setText("DSD MIXER | LEVEL 1", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffE2E5EC));
        addAndMakeVisible(titleLabel);

        auto styleTopBtn = [](juce::TextButton& btn)
        {
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252830));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff323640));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffD0D4DC));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        };

        styleTopBtn(settingsBtn);
        settingsBtn.onClick = [this]() { openAudioSettingsDialog(); };
        addAndMakeVisible(settingsBtn);

        styleTopBtn(matrixBtn);
        matrixBtn.onClick = [this]() { openRoutingMatrixDialog(); };
        addAndMakeVisible(matrixBtn);

        styleTopBtn(perfBtn);
        perfBtn.onClick = [this]() { openPerformanceDialog(); };
        addAndMakeVisible(perfBtn);

        styleTopBtn(stageInspectorBtn);
        stageInspectorBtn.onClick = [this]() { openStageInspectorDialog(); };
        addAndMakeVisible(stageInspectorBtn);

        styleTopBtn(remoteBtn);
        remoteBtn.onClick = [this]() { openControlServerDialog(); };
        addAndMakeVisible(remoteBtn);

        styleTopBtn(toolsBtn);
        toolsBtn.setTooltip("Scenes, auto-ducking, output loudness & limiters");
        toolsBtn.onClick = [this]() { showToolsMenu(); };
        addAndMakeVisible(toolsBtn);

        styleTopBtn(saveSessionBtn);
        saveSessionBtn.onClick = [this]() { onSaveSessionClicked(); };
        addAndMakeVisible(saveSessionBtn);

        styleTopBtn(loadSessionBtn);
        loadSessionBtn.onClick = [this]() { onLoadSessionClicked(); };
        addAndMakeVisible(loadSessionBtn);

        sampleRateLabel.setText("48000 Hz", juce::dontSendNotification);
        sampleRateLabel.setJustificationType(juce::Justification::centred);
        sampleRateLabel.setFont(juce::FontOptions(10.0f));
        sampleRateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        sampleRateLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9CA3AF));
        addAndMakeVisible(sampleRateLabel);

        bufferSizeLabel.setText("128 smp (2.67 ms)", juce::dontSendNotification);
        bufferSizeLabel.setJustificationType(juce::Justification::centred);
        bufferSizeLabel.setFont(juce::FontOptions(10.0f));
        bufferSizeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9CA3AF));
        addAndMakeVisible(bufferSizeLabel);

        engineStatusBadge.setText("AUDIO OK", juce::dontSendNotification);
        engineStatusBadge.setJustificationType(juce::Justification::centred);
        engineStatusBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161A22));
        engineStatusBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xff3B4252));
        engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffD8DEE9));
        addAndMakeVisible(engineStatusBadge);

        cpuLoadLabel.setText("DSP: 0.0%", juce::dontSendNotification);
        cpuLoadLabel.setJustificationType(juce::Justification::centred);
        cpuLoadLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        cpuLoadLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        cpuLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentAmber());
        addAndMakeVisible(cpuLoadLabel);

        updateStats();
    }

    void TopBar::openAudioSettingsDialog()
    {
        auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManagerRef.getJuceManager(),
            1, 16,
            1, 8,
            false, false, false, false);

        selector->setSize(520, 440);

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(selector.release());
        opts.dialogTitle = "DSD Mixer - Audio Device Setup";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;

        opts.launchAsync();
    }

    void TopBar::openRoutingMatrixDialog()
    {
        auto dlg = std::make_unique<RoutingMatrixDialog>(audioEngineRef.getRoutingEngine(),
                                                        audioEngineRef.getChannelManager(),
                                                        audioEngineRef.getOutputManager());

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(dlg.release());
        opts.dialogTitle = "DSD Mixer - 16x4 Routing Matrix";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;

        opts.launchAsync();
    }

    void TopBar::openPerformanceDialog()
    {
        auto dlg = std::make_unique<PerformanceMonitorDialog>(audioEngineRef);

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(dlg.release());
        opts.dialogTitle = "DSD Mixer - Multicore Performance Monitor";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;

        opts.launchAsync();
    }

    void TopBar::showToolsMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Scenes...");
        menu.addItem(2, "Auto-Ducking...");
        menu.addItem(3, "Output Loudness & Limiters...");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&toolsBtn),
            [this](int result)
            {
                if (result == 1)
                    openToolDialog(std::make_unique<ScenesDialog>(audioEngineRef), "DSD Mixer - Scenes");
                else if (result == 2)
                    openToolDialog(std::make_unique<AutoDuckDialog>(audioEngineRef), "DSD Mixer - Auto-Ducking");
                else if (result == 3)
                    openToolDialog(std::make_unique<LoudnessDialog>(audioEngineRef), "DSD Mixer - Output Loudness & Limiters");
            });
    }

    void TopBar::openToolDialog(std::unique_ptr<juce::Component> content, const juce::String& title)
    {
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(content.release());
        opts.dialogTitle = title;
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;
        opts.launchAsync();
    }

    void TopBar::openStageInspectorDialog()
    {
        auto dlg = std::make_unique<StageInspectorDialog>(
            audioEngineRef.getChannelManager(),
            audioEngineRef.getOutputManager(),
            audioEngineRef.getScheduler()
        );

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(dlg.release());
        opts.dialogTitle = "DSD Mixer - Signal Stage Inspector & Diagnostics";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = true;

        opts.launchAsync();
    }

    void TopBar::openControlServerDialog()
    {
        if (controlServerRef == nullptr)
            return;

        auto dlg = std::make_unique<ControlServerDialog>(*controlServerRef);

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(dlg.release());
        opts.dialogTitle = "DSD Mixer - Remote Control Surface & Pairing";
        opts.componentToCentreAround = this;
        opts.dialogBackgroundColour = DSDLookAndFeel::getConsoleDarkBg();
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;

        opts.launchAsync();
    }

    void TopBar::onSaveSessionClicked()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Save DSD Mixer Session",
                                                            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                            "*.dsd");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    if (file.getFileExtension() != ".dsd")
                        file = file.withFileExtension("dsd");

                    bool success = SessionManager::saveSessionToFile(file, audioEngineRef, deviceManagerRef);
                    if (success)
                    {
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Session Saved",
                            "Session saved to " + file.getFileName(), "OK");
                    }
                }
            });
    }

    void TopBar::onLoadSessionClicked()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Load DSD Mixer Session",
                                                            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                            "*.dsd");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    bool success = SessionManager::loadSessionFromFile(file, audioEngineRef, deviceManagerRef);
                    if (success)
                    {
                        if (onSessionLoaded)
                            onSessionLoaded();

                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Session Loaded",
                            "Session loaded from " + file.getFileName(), "OK");
                    }
                }
            });
    }

    void TopBar::updateStats()
    {
        const auto& stats = audioEngineRef.getPerformanceStats();
        const float cpu = stats.cpuLoadPercent.load(std::memory_order_relaxed);
        const double procMs = stats.processingTimeMs.load(std::memory_order_relaxed);
        const double deadMs = stats.deadlineMs.load(std::memory_order_relaxed);
        const uint64_t xruns = stats.xrunCount.load(std::memory_order_relaxed);
        const bool glitch = stats.isGlitching.load(std::memory_order_relaxed);

        cpuLoadLabel.setText(juce::String::formatted("DSP: %.1f%% (%.2f ms)", cpu, procMs), juce::dontSendNotification);

        const double sr = deviceManagerRef.getCurrentSampleRate();
        const int bs = deviceManagerRef.getCurrentBufferSize();
        sampleRateLabel.setText(juce::String::formatted("%.0f Hz", sr), juce::dontSendNotification);
        bufferSizeLabel.setText(juce::String::formatted("%d smp (%.2f ms)", bs, (bs / sr) * 1000.0), juce::dontSendNotification);

        if (controlServerRef != nullptr)
        {
            if (controlServerRef->isRunning())
            {
                int authClients = controlServerRef->getAuthenticatedClientCount();
                if (authClients > 0)
                {
                    remoteBtn.setButtonText(juce::String::formatted("REMOTE (%d)", authClients));
                    remoteBtn.setToggleState(true, juce::dontSendNotification);
                }
                else
                {
                    remoteBtn.setButtonText("REMOTE (0)");
                    remoteBtn.setToggleState(false, juce::dontSendNotification);
                }
            }
            else
            {
                remoteBtn.setButtonText("REMOTE (OFF)");
                remoteBtn.setToggleState(false, juce::dontSendNotification);
            }
        }

        if (glitch || xruns > 0)
        {
            engineStatusBadge.setText(juce::String::formatted("GLITCH (%llu)", xruns), juce::dontSendNotification);
            engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff3B1414));
            engineStatusBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xffFF3B30));
            engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffFF453A));
        }
        else
        {
            engineStatusBadge.setText("AUDIO OK", juce::dontSendNotification);
            engineStatusBadge.setColour(juce::Label::backgroundColourId, juce::Colour(0xff161A22));
            engineStatusBadge.setColour(juce::Label::outlineColourId, juce::Colour(0xff3B4252));
            engineStatusBadge.setColour(juce::Label::textColourId, juce::Colour(0xffD8DEE9));
        }

        repaint();
    }

    void TopBar::resized()
    {
        const int barH = getHeight();
        const int btnH = 26;
        const int btnY = (barH - btnH) / 2;

        int leftX = 12;

        // Title on the left
        titleLabel.setBounds(leftX, btnY, 155, btnH);
        leftX += 165;

        // Configuration & tool buttons (26px uniform height)
        settingsBtn.setBounds(leftX, btnY, 100, btnH);
        leftX += 105;

        matrixBtn.setBounds(leftX, btnY, 115, btnH);
        leftX += 120;

        perfBtn.setBounds(leftX, btnY, 100, btnH);
        leftX += 105;

        stageInspectorBtn.setBounds(leftX, btnY, 105, btnH);
        leftX += 112;

        remoteBtn.setBounds(leftX, btnY, 100, btnH);
        leftX += 108;

        // Vertical divider separator
        sessionDividerX = static_cast<float>(leftX + 4);
        leftX += 14;

        // Session buttons
        saveSessionBtn.setBounds(leftX, btnY, 80, btnH);
        leftX += 85;

        loadSessionBtn.setBounds(leftX, btnY, 80, btnH);
        leftX += 88;

        toolsBtn.setBounds(leftX, btnY, 64, btnH);

        // Technical status & diagnostics pinned to the right
        int rightX = getWidth() - 12;

        const int podW = 362;
        statusPodBounds = juce::Rectangle<float>(static_cast<float>(rightX - podW), static_cast<float>(btnY), static_cast<float>(podW), static_cast<float>(btnH));

        // Layout items from right to left inside status pod
        cpuLoadLabel.setBounds(rightX - 96, btnY, 92, btnH);
        rightX -= 100;

        engineStatusBadge.setBounds(rightX - 80, btnY + 2, 76, btnH - 4);
        rightX -= 84;

        bufferSizeLabel.setBounds(rightX - 104, btnY, 100, btnH);
        rightX -= 106;

        sampleRateLabel.setBounds(rightX - 70, btnY, 66, btnH);
    }

    void TopBar::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        // Top bar chassis background
        g.setColour(juce::Colour(0xff181A20));
        g.fillRect(bounds);

        // Bottom chassis border
        g.setColour(juce::Colour(0xff2A2E38));
        g.fillRect(0.0f, bounds.getBottom() - 1.2f, bounds.getWidth(), 1.2f);

        // Session divider line
        if (sessionDividerX > 0.0f)
        {
            g.setColour(juce::Colour(0xff2E323C));
            g.drawVerticalLine(static_cast<int>(sessionDividerX), 11.0f, bounds.getBottom() - 11.0f);
        }

        // Recessed OLED status pod
        if (statusPodBounds.getWidth() > 0.0f)
        {
            g.setColour(juce::Colour(0xff0A0B0E));
            g.fillRoundedRectangle(statusPodBounds, 3.5f);
            g.setColour(juce::Colour(0xff252932));
            g.drawRoundedRectangle(statusPodBounds, 3.5f, 1.0f);

            // Subtle vertical dividers between status fields
            g.setColour(juce::Colour(0xff1A1D24));
            float div1 = statusPodBounds.getX() + 72.0f;
            float div2 = div1 + 124.0f;
            float div3 = div2 + 84.0f;
            g.drawVerticalLine(static_cast<int>(div1), statusPodBounds.getY() + 4.0f, statusPodBounds.getBottom() - 4.0f);
            g.drawVerticalLine(static_cast<int>(div2), statusPodBounds.getY() + 4.0f, statusPodBounds.getBottom() - 4.0f);
            g.drawVerticalLine(static_cast<int>(div3), statusPodBounds.getY() + 4.0f, statusPodBounds.getBottom() - 4.0f);
        }
    }
} // namespace dsd
