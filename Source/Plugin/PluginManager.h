#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

namespace dsd
{
    class PluginManager
    {
    public:
        struct Listener
        {
            virtual ~Listener() = default;
            virtual void pluginListChanged() = 0;
            virtual void scanProgressUpdated(const juce::String& pluginName, float progress) {}
        };

        static PluginManager& getInstance();

        void initialize();
        void startBackgroundAutoScan();
        void rescanAllAsync(std::function<void(int totalFound)> onComplete = nullptr);

        bool isScanning() const noexcept { return scanningFlag.load(); }
        juce::String getScanningStatus() const;
        int getNumKnownPlugins() const;

        void addListener(Listener* listener);
        void removeListener(Listener* listener);

        const juce::KnownPluginList& getKnownPluginList() const noexcept { return knownPluginList; }
        juce::KnownPluginList& getKnownPluginList() noexcept { return knownPluginList; }

        juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

        std::unique_ptr<juce::AudioPluginInstance> loadPlugin(const juce::PluginDescription& desc,
                                                              juce::String& errorMessage);

        std::unique_ptr<juce::AudioPluginInstance> loadPluginFromFile(const juce::File& file,
                                                                      juce::String& errorMessage);

    private:
        PluginManager() = default;
        ~PluginManager();

        void loadCache();
        void saveCache();
        juce::File getCacheFile() const;
        juce::File getDeadMansPedalFile() const;

        juce::AudioPluginFormatManager formatManager;
        juce::KnownPluginList knownPluginList;
        bool isInitialized{false};

        std::atomic<bool> scanningFlag{false};
        std::atomic<float> currentProgress{0.0f};
        juce::String currentScanningName;
        mutable std::mutex scanStatusMutex;

        std::unique_ptr<std::thread> scanThread;
        juce::ListenerList<Listener> listeners;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
    };
} // namespace dsd

