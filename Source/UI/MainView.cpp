#include "UI/MainView.h"

namespace dsd
{
    MainView::MainView(AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : deviceManagerRef(devManager),
          audioEngineRef(audioEngine),
          topBar(devManager, audioEngine),
          outputBayPanel(audioEngine.getOutputManager())
    {
        setLookAndFeel(&customLookAndFeel);

        addAndMakeVisible(topBar);

        // Instantiate 16 Channel Strips
        auto& channelMgr = audioEngineRef.getChannelManager();
        const int numChannels = channelMgr.getNumChannels();

        for (int i = 0; i < numChannels; ++i)
        {
            if (auto* ch = channelMgr.getChannel(i))
            {
                auto strip = std::make_unique<ChannelStrip>(*ch);
                channelsContainer.addAndMakeVisible(strip.get());
                channelStrips.push_back(std::move(strip));
            }
        }

        channelsViewport.setViewedComponent(&channelsContainer, false);
        channelsViewport.setScrollBarsShown(false, true); // Show horizontal scrollbar
        addAndMakeVisible(channelsViewport);

        // Output Bay Panel on the right (4 configurable output strips)
        addAndMakeVisible(outputBayPanel);

        // UI Refresh Timer @ 45 Hz
        startTimerHz(45);

        setSize(1360, 720);
    }

    MainView::~MainView()
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void MainView::timerCallback()
    {
        topBar.updateStats();

        for (auto& strip : channelStrips)
        {
            if (strip != nullptr)
                strip->updateMeterFromAudio();
        }

        outputBayPanel.updateMeters();
    }

    void MainView::resized()
    {
        auto bounds = getLocalBounds();

        // 1. Top Bar
        topBar.setBounds(bounds.removeFromTop(44));

        bounds.reduce(10, 10);

        // 2. Output Bay Panel on right (4 Output Strips, approx 420 px width)
        const int outputPanelWidth = 420;
        outputBayPanel.setBounds(bounds.removeFromRight(outputPanelWidth));

        bounds.removeFromRight(10); // Gap between Input Bay and Output Bay

        // 3. Input Bay Viewport on left
        channelsViewport.setBounds(bounds);

        const int stripWidth = 118;
        const int stripGap = 6;
        const int numStrips = static_cast<int>(channelStrips.size());
        const int totalWidth = numStrips * stripWidth + (numStrips - 1) * stripGap;

        channelsContainer.setBounds(0, 0, std::max(totalWidth, bounds.getWidth()), bounds.getHeight());

        int currentX = 0;
        for (auto& strip : channelStrips)
        {
            if (strip != nullptr)
            {
                strip->setBounds(currentX, 0, stripWidth, bounds.getHeight());
                currentX += stripWidth + stripGap;
            }
        }
    }

    void MainView::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }
} // namespace dsd
