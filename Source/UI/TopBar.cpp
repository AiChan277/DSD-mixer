#include "UI/TopBar.h"
#include "UI/DSDLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace dsd
{
    TopBar::TopBar(AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : deviceManagerRef(devManager), audioEngineRef(audioEngine)
    {
        // 1. Logo & App Title
        titleLabel.setText("DSD MIXER", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(titleLabel);

        // 2. Audio Device Settings Button
        settingsBtn.setColour(juce::TextButton::buttonColourId, DSDLookAndFeel::getConsoleBevel());
        settingsBtn.onClick = [this]() { openAudioSettingsDialog(); };
        addAndMakeVisible(settingsBtn);

        // 3. Sample Rate Display
        sampleRateLabel.setText("48000 Hz", juce::dontSendNotification);
        sampleRateLabel.setJustificationType(juce::Justification::centred);
        sampleRateLabel.setFont(juce::FontOptions(11.0f));
        sampleRateLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        sampleRateLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(sampleRateLabel);

        // 4. Buffer Size Display
        bufferSizeLabel.setText("128 smp (2.67 ms)", juce::dontSendNotification);
        bufferSizeLabel.setJustificationType(juce::Justification::centred);
        bufferSizeLabel.setFont(juce::FontOptions(11.0f));
        bufferSizeLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        bufferSizeLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextSecondary());
        addAndMakeVisible(bufferSizeLabel);

        // 5. Real-Time Engine Status Badge ("AUDIO OK" / "GLITCH")
        engineStatusBadge.setText("AUDIO OK", juce::dontSendNotification);
        engineStatusBadge.setJustificationType(juce::Justification::centred);
        engineStatusBadge.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        engineStatusBadge.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getAccentGreen().withAlpha(0.25f));
        engineStatusBadge.setColour(juce::Label::outlineColourId, DSDLookAndFeel::getAccentGreen());
        engineStatusBadge.setColour(juce::Label::textColourId, DSDLookAndFeel::getAccentGreen());
        addAndMakeVisible(engineStatusBadge);

        // 6. CPU Load Readout
        cpuLoadLabel.setText("DSP: 0.0%", juce::dontSendNotification);
        cpuLoadLabel.setJustificationType(juce::Justification::centred);
        cpuLoadLabel.setFont(juce::FontOptions(11.0f));
        cpuLoadLabel.setColour(juce::Label::backgroundColourId, DSDLookAndFeel::getOledBlack());
        cpuLoadLabel.setColour(juce::Label::textColourId, DSDLookAndFeel::getTextPrimary());
        addAndMakeVisible(cpuLoadLabel);
    }

    void TopBar::openAudioSettingsDialog()
    {
        auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManagerRef.getJuceManager(),
            1, 2,  // min/max input channels
            1, 2,  // min/max output channels
            false, false, false, false);

        selector->setSize(500, 420);

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

    void TopBar::updateStats()
    {
        const auto& stats = audioEngineRef.getPerformanceStats();
        const float cpu = stats.cpuLoadPercent.load(std::memory_order_relaxed);
        const double procMs = stats.processingTimeMs.load(std::memory_order_relaxed);
        const double deadMs = stats.deadlineMs.load(std::memory_order_relaxed);
        const uint64_t xruns = stats.xrunCount.load(std::memory_order_relaxed);
        const bool glitch = stats.isGlitching.load(std::memory_order_relaxed);

        currentCpuPercent = std::clamp(cpu, 0.0f, 100.0f);

        // Update Labels
        cpuLoadLabel.setText(juce::String::formatted("DSP: %.1f%% (%.2f/%.2f ms)", cpu, procMs, deadMs), juce::dontSendNotification);

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

        titleLabel.setBounds(bounds.removeFromLeft(120));
        bounds.removeFromLeft(12);

        settingsBtn.setBounds(bounds.removeFromLeft(120));
        bounds.removeFromLeft(8);

        sampleRateLabel.setBounds(bounds.removeFromLeft(80));
        bounds.removeFromLeft(6);

        bufferSizeLabel.setBounds(bounds.removeFromLeft(130));
        bounds.removeFromLeft(10);

        engineStatusBadge.setBounds(bounds.removeFromLeft(100));
        bounds.removeFromLeft(10);

        cpuLoadLabel.setBounds(bounds.removeFromLeft(160));
    }

    void TopBar::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Top bar panel background
        g.setColour(DSDLookAndFeel::getConsolePanelBg());
        g.fillRect(bounds);

        // Bottom separator border
        g.setColour(DSDLookAndFeel::getConsoleBevel());
        g.fillRect(0.0f, bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);
    }
} // namespace dsd
