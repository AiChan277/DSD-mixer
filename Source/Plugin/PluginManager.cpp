#include "Plugin/PluginManager.h"

namespace dsd
{
    PluginManager& PluginManager::getInstance()
    {
        static PluginManager instance;
        return instance;
    }

    void PluginManager::initialize()
    {
        if (isInitialized)
            return;

        // Register standard VST3 format
        formatManager.addDefaultFormats();
        isInitialized = true;
    }

    void PluginManager::scanDefaultVST3Folders()
    {
        initialize();

        // Standard Windows VST3 directory
        juce::File vst3Dir("C:\\Program Files\\Common Files\\VST3");
        if (vst3Dir.exists() && vst3Dir.isDirectory())
        {
            scanFolderAsync(vst3Dir);
        }
    }

    void PluginManager::scanFolderAsync(const juce::File& folder, std::function<void(int totalFound)> onComplete)
    {
        initialize();

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat(i);
            if (format != nullptr && format->canScanForPlugins())
            {
                juce::PluginDirectoryScanner scanner(knownPluginList,
                                                     *format,
                                                     juce::FileSearchPath(folder.getFullPathName()),
                                                     true, // recursive
                                                     juce::File());

                juce::String pluginBeingScanned;
                while (scanner.scanNextFile(true, pluginBeingScanned))
                {
                    // Scanning progress
                }
            }
        }

        if (onComplete)
            onComplete(knownPluginList.getNumTypes());
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginManager::loadPlugin(const juce::PluginDescription& desc,
                                                                         juce::String& errorMessage)
    {
        initialize();
        double sampleRate = 48000.0;
        int blockSize = 128;

        return formatManager.createPluginInstance(desc, sampleRate, blockSize, errorMessage);
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginManager::loadPluginFromFile(const juce::File& file,
                                                                                 juce::String& errorMessage)
    {
        initialize();

        juce::OwnedArray<juce::PluginDescription> descriptions;
        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat(i);
            if (format != nullptr && format->canScanForPlugins())
            {
                format->findAllTypesForFile(descriptions, file.getFullPathName());
                if (!descriptions.isEmpty())
                    break;
            }
        }

        if (descriptions.isEmpty())
        {
            errorMessage = "No valid plugin found in file: " + file.getFileName();
            return nullptr;
        }

        return loadPlugin(*descriptions.getFirst(), errorMessage);
    }
} // namespace dsd
