#include "UI/MainView.h"

namespace dsd
{
    MainView::MainView(AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : deviceManagerRef(devManager),
          audioEngineRef(audioEngine),
          topBar(devManager, audioEngine),
          outputBayPanel(audioEngine.getOutputManager(), devManager.getJuceManager())
    {
        setLookAndFeel(&customLookAndFeel);

        addAndMakeVisible(topBar);

        auto& channelMgr = audioEngineRef.getChannelManager();
        const int numChannels = channelMgr.getNumChannels();

        for (int i = 0; i < numChannels; ++i)
        {
            if (auto* ch = channelMgr.getChannel(i))
            {
                auto strip = std::make_unique<ChannelStrip>(*ch, deviceManagerRef.getJuceManager());
                channelsContainer.addAndMakeVisible(strip.get());
                channelStrips.push_back(std::move(strip));
            }
        }

        channelsViewport.setViewedComponent(&channelsContainer, false);
        channelsViewport.setScrollBarsShown(false, true);
        addAndMakeVisible(channelsViewport);

        addAndMakeVisible(outputBayPanel);

        topBar.onSessionLoaded = [this]()
        {
            updateAllUI();
        };

        startTimerHz(45);
        setSize(1360, 720);
    }

    MainView::~MainView()
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void MainView::updateAllUI()
    {
        for (auto& strip : channelStrips)
        {
            if (strip != nullptr)
                strip->updateUIFromChannel();
        }
        outputBayPanel.updateAllUI();
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
        topBar.setBounds(bounds.removeFromTop(44));
        bounds.reduce(10, 10);

        const int outputPanelWidth = 420;
        outputBayPanel.setBounds(bounds.removeFromRight(outputPanelWidth));

        bounds.removeFromRight(10);
        channelsViewport.setBounds(bounds);

        const int stripWidth = 118;
        const int stripGap = 6;
        const int numStrips = static_cast<int>(channelStrips.size());
        const int totalWidth = numStrips * stripWidth + (numStrips - 1) * stripGap;

        // Viewport scrollbar height is 16px when horizontal scrollbar is shown
        const bool willShowScrollbar = (totalWidth > bounds.getWidth());
        const int scrollbarPad = willShowScrollbar ? 16 : 0;
        const int availableHeight = bounds.getHeight() - scrollbarPad;

        channelsContainer.setBounds(0, 0, std::max(totalWidth, bounds.getWidth()), availableHeight);

        int currentX = 0;
        for (auto& strip : channelStrips)
        {
            if (strip != nullptr)
            {
                strip->setBounds(currentX, 0, stripWidth, availableHeight);
                currentX += stripWidth + stripGap;
            }
        }
    }

    void MainView::paint(juce::Graphics& g)
    {
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }
} // namespace dsd
