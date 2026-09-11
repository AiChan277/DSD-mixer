#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace dsd
{
    class PluginManager
    {
    public:
        static PluginManager& getInstance();

        void initialize();
        void scanDefaultVST3Folders();
        void scanFolderAsync(const juce::File& folder, std::function<void(int totalFound)> onComplete = nullptr);

        const juce::KnownPluginList& getKnownPluginList() const noexcept { return knownPluginList; }
        juce::KnownPluginList& getKnownPluginList() noexcept { return knownPluginList; }

        juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

        std::unique_ptr<juce::AudioPluginInstance> loadPlugin(const juce::PluginDescription& desc,
                                                              juce::String& errorMessage);

        std::unique_ptr<juce::AudioPluginInstance> loadPluginFromFile(const juce::File& file,
                                                                      juce::String& errorMessage);

    private:
        PluginManager() = default;
        ~PluginManager() = default;

        juce::AudioPluginFormatManager formatManager;
        juce::KnownPluginList knownPluginList;
        bool isInitialized{false};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
    };
} // namespace dsd
