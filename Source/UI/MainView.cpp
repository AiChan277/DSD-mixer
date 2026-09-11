#include "UI/MainView.h"

namespace dsd
{
    MainView::MainView(AudioDeviceManager& devManager, AudioEngine& audioEngine)
        : deviceManagerRef(devManager),
          audioEngineRef(audioEngine),
          topBar(devManager, audioEngine),
          masterStrip(audioEngine.getMasterBus())
    {
        setLookAndFeel(&customLookAndFeel);

        addAndMakeVisible(topBar);

        // Instantiate Channel Strips for all channels
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
        channelsViewport.setScrollBarsShown(false, true); // show horizontal scrollbar if needed
        addAndMakeVisible(channelsViewport);

        addAndMakeVisible(masterStrip);

        // Refresh UI at ~45 FPS (approx 22 ms timer interval)
        startTimerHz(45);

        setSize(980, 680);
    }

    MainView::~MainView()
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void MainView::timerCallback()
    {
        // Poll lock-free atomics from audio thread
        topBar.updateStats();

        for (auto& strip : channelStrips)
        {
            if (strip != nullptr)
                strip->updateMeterFromAudio();
        }

        masterStrip.updateMeterFromAudio();
    }

    void MainView::resized()
    {
        auto bounds = getLocalBounds();

        // 1. Top status bar
        topBar.setBounds(bounds.removeFromTop(44));

        // Margins for main console surface
        bounds.reduce(12, 12);

        // 2. Output Bay (Master Strip on right)
        const int masterWidth = 140;
        masterStrip.setBounds(bounds.removeFromRight(masterWidth));

        bounds.removeFromRight(12); // gap between channels and master

        // 3. Input Bay (Channels Viewport on left)
        channelsViewport.setBounds(bounds);

        // Calculate layout of channels container
        const int stripWidth = 130;
        const int stripGap = 8;
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
        // Dark metallic console body
        g.fillAll(DSDLookAndFeel::getConsoleDarkBg());
    }
} // namespace dsd
