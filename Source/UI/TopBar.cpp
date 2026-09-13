#include "UI/TopBar.h"
#include "UI/DSDLookAndFeel.h"
#include "UI/RoutingMatrixDialog.h"
#include "UI/PerformanceMonitorDialog.h"
#include "UI/StageInspectorDialog.h"
#include "Session/SessionManager.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace dsd
{
    TopBar::TopBar(AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : deviceManagerRef(devManager), audioEngineRef(audioEngine)
    {
        titleLabel.setText("DSD MIXER | LEVEL 1", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        settingsBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        settingsBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        settingsBtn.onClick = [this]() { openAudioSettingsDialog(); };
        addAndMakeVisible(settingsBtn);

        matrixBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentBlue());
        matrixBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffFFFFFF));
        matrixBtn.onClick = [this]() { openRoutingMatrixDialog(); };
        addAndMakeVisible(matrixBtn);

        perfBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        perfBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        perfBtn.onClick = [this]() { openPerformanceDialog(); };
        addAndMakeVisible(perfBtn);

        stageInspectorBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getAccentAmber());
        stageInspectorBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff180800));
        stageInspectorBtn.onClick = [this]() { openStageInspectorDialog(); };
        addAndMakeVisible(stageInspectorBtn);

        saveSessionBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        saveSessionBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        saveSessionBtn.onClick = [this]() { onSaveSessionClicked(); };
        addAndMakeVisible(saveSessionBtn);

        loadSessionBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        loadSessionBtn.setColour(juce::TextButton::textColourOffId, DSDLookAndFeel::getTextPrimary());
        loadSessionBtn.onClick = [this]() { onLoadSessionClicked(); };
        addAndMakeVisible(loadSessionBtn);

        sampleRateLabel.setText("48000 Hz", juce::dontSendNotification);
        sampleRateLabel.setJustificationType(juce::Justification::centred);
        sampleRateLabel.setFont(juce::FontOptions(11.0f));
        sampleRateLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        sampleRateLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(sampleRateLabel);

        bufferSizeLabel.setText("128 smp (2.67 ms)", juce::dontSendNotification);
        bufferSizeLabel.setJustificationType(juce::Justification::centred);
        bufferSizeLabel.setFont(juce::FontOptions(11.0f));
        bufferSizeLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        bufferSizeLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(bufferSizeLabel);

        engineStatusBadge.setText("AUDIO OK", juce::dontSendNotification);
        engineStatusBadge.setJustificationType(juce::Justification::centred);
        engineStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentGreen().withAlpha(0.25f));
        engineStatusBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentGreen());
        engineStatusBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(engineStatusBadge);

        cpuLoadLabel.setText("DSP: 0.0%", juce::dontSendNotification);
        cpuLoadLabel.setJustificationType(juce::Justification::centred);
        cpuLoadLabel.setFont(juce::FontOptions(11.0f));
        cpuLoadLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        cpuLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextOnOled());
        addAndMakeVisible(cpuLoadLabel);
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

        if (glitch || xruns > 0)
        {
            engineStatusBadge.setText(juce::String::formatted("GLITCH (%llu)", xruns), juce::dontSendNotification);
            engineStatusBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentRed().withAlpha(0.25f));
            engineStatusBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentRed());
            engineStatusBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentRed());
        }
        else
        {
            engineStatusBadge.setText("AUDIO OK", juce::dontSendNotification);
            engineStatusBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentGreen().withAlpha(0.25f));
            engineStatusBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentGreen());
            engineStatusBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        }

        repaint();
    }

    void TopBar::resized()
    {
        auto bounds = getLocalBounds().reduced(8, 6);

        titleLabel.setBounds(bounds.removeFromLeft(160));
        bounds.removeFromLeft(10);

        settingsBtn.setBounds(bounds.removeFromLeft(105));
        bounds.removeFromLeft(6);

        matrixBtn.setBounds(bounds.removeFromLeft(125));
        bounds.removeFromLeft(6);

        perfBtn.setBounds(bounds.removeFromLeft(110));
        bounds.removeFromLeft(6);

        stageInspectorBtn.setBounds(bounds.removeFromLeft(115));
        bounds.removeFromLeft(6);

        saveSessionBtn.setBounds(bounds.removeFromLeft(85));
        bounds.removeFromLeft(4);

        loadSessionBtn.setBounds(bounds.removeFromLeft(85));
        bounds.removeFromLeft(10);

        sampleRateLabel.setBounds(bounds.removeFromLeft(75));
        bounds.removeFromLeft(6);

        bufferSizeLabel.setBounds(bounds.removeFromLeft(120));
        bounds.removeFromLeft(8);

        engineStatusBadge.setBounds(bounds.removeFromLeft(95));
        bounds.removeFromLeft(8);

        cpuLoadLabel.setBounds(bounds.removeFromLeft(140));
    }

    void TopBar::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(DSDLookAndFeel::getConsolePanelBg());
        g.fillRect(bounds);

        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.fillRect(0.0f, bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
    }
} // namespace dsd
