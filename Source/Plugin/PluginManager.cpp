#include "Plugin/PluginManager.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace dsd
{
    PluginManager& PluginManager::getInstance()
    {
        static PluginManager instance;
        return instance;
    }

    PluginManager::~PluginManager()
    {
        if (scanThread != nullptr && scanThread->joinable())
        {
            scanThread->join();
        }
    }

    juce::File PluginManager::getCacheFile() const
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("DSDMixer")
            .getChildFile("vst_cache.xml");
    }

    juce::File PluginManager::getDeadMansPedalFile() const
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("DSDMixer")
            .getChildFile("vst_crash_pedal.txt");
    }

    void PluginManager::loadCache()
    {
        auto file = getCacheFile();
        if (file.existsAsFile())
        {
            juce::XmlDocument doc(file);
            if (auto xml = doc.getDocumentElement())
            {
                knownPluginList.recreateFromXml(*xml);
            }
        }
    }

    void PluginManager::saveCache()
    {
        auto file = getCacheFile();
        file.getParentDirectory().createDirectory();
        if (auto xml = knownPluginList.createXml())
        {
            xml->writeTo(file);
        }
    }

    void PluginManager::initialize()
    {
        if (isInitialized)
            return;

        // 1. Register standard VST3 formats
        formatManager.addDefaultFormats();

        // 2. Load cached plugins from previous session for instant availability
        loadCache();

        isInitialized = true;
    }

    void PluginManager::addListener(Listener* listener)
    {
        listeners.add(listener);
    }

    void PluginManager::removeListener(Listener* listener)
    {
        listeners.remove(listener);
    }

    juce::String PluginManager::getScanningStatus() const
    {
        std::lock_guard<std::mutex> lock(scanStatusMutex);
        if (!scanningFlag.load())
            return "Scan idle";
        return "Scanning: " + currentScanningName;
    }

    int PluginManager::getNumKnownPlugins() const
    {
        return knownPluginList.getNumTypes();
    }

    void PluginManager::startBackgroundAutoScan()
    {
        initialize();

        if (scanningFlag.load())
            return; // Already scanning

        if (scanThread != nullptr && scanThread->joinable())
            scanThread->join();

        scanningFlag.store(true);

        scanThread = std::make_unique<std::thread>([this]()
        {
            // Build comprehensive list of VST3 directories
            juce::FileSearchPath searchPath;

            // 1. Standard 64-bit VST3
            juce::File v64("C:\\Program Files\\Common Files\\VST3");
            if (v64.exists()) searchPath.add(v64);

            // 2. Standard 32-bit VST3 (if present)
            juce::File v32("C:\\Program Files (x86)\\Common Files\\VST3");
            if (v32.exists()) searchPath.add(v32);

            // 3. User-level LocalAppData VST3
            auto localApp = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getParentDirectory();
            juce::File userVst3 = localApp.getChildFile("Local/Programs/Common/VST3");
            if (userVst3.exists()) searchPath.add(userVst3);

            // 4. Default locations from JUCE format manager
            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* fmt = formatManager.getFormat(i);
                if (fmt != nullptr)
                {
                    searchPath.addPath(fmt->getDefaultLocationsToSearch());
                }
            }

            auto deadMan = getDeadMansPedalFile();
            deadMan.getParentDirectory().createDirectory();

            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* format = formatManager.getFormat(i);
                if (format != nullptr && format->canScanForPlugins())
                {
                    // Apply blacklists from previous crash if any
                    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal(knownPluginList, deadMan);

                    juce::PluginDirectoryScanner scanner(knownPluginList,
                                                         *format,
                                                         searchPath,
                                                         true, // recursive
                                                         deadMan);

                    juce::String pluginScanned;
                    while (scanner.scanNextFile(true, pluginScanned))
                    {
                        {
                            std::lock_guard<std::mutex> lock(scanStatusMutex);
                            currentScanningName = pluginScanned;
                            currentProgress.store(scanner.getProgress());
                        }

                        juce::MessageManager::callAsync([this, pluginScanned, prog = scanner.getProgress()]()
                        {
                            listeners.call(&Listener::scanProgressUpdated, pluginScanned, prog);
                        });
                    }
                }
            }

            // Save cache to disk
            saveCache();
            scanningFlag.store(false);

            // Notify UI on message thread
            juce::MessageManager::callAsync([this]()
            {
                listeners.call(&Listener::pluginListChanged);
            });
        });
    }

    void PluginManager::rescanAllAsync(std::function<void(int totalFound)> onComplete)
    {
        initialize();

        if (scanningFlag.load())
            return;

        if (scanThread != nullptr && scanThread->joinable())
            scanThread->join();

        scanningFlag.store(true);

        scanThread = std::make_unique<std::thread>([this, onComplete]()
        {
            juce::FileSearchPath searchPath;
            juce::File v64("C:\\Program Files\\Common Files\\VST3");
            if (v64.exists()) searchPath.add(v64);
            juce::File v32("C:\\Program Files (x86)\\Common Files\\VST3");
            if (v32.exists()) searchPath.add(v32);

            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* fmt = formatManager.getFormat(i);
                if (fmt != nullptr) searchPath.addPath(fmt->getDefaultLocationsToSearch());
            }

            auto deadMan = getDeadMansPedalFile();

            for (int i = 0; i < formatManager.getNumFormats(); ++i)
            {
                auto* format = formatManager.getFormat(i);
                if (format != nullptr && format->canScanForPlugins())
                {
                    juce::PluginDirectoryScanner scanner(knownPluginList,
                                                         *format,
                                                         searchPath,
                                                         true,
                                                         deadMan);

                    juce::String pluginScanned;
                    while (scanner.scanNextFile(false, pluginScanned)) // force re-scan
                    {
                        {
                            std::lock_guard<std::mutex> lock(scanStatusMutex);
                            currentScanningName = pluginScanned;
                        }
                    }
                }
            }

            saveCache();
            scanningFlag.store(false);

            juce::MessageManager::callAsync([this, onComplete]()
            {
                listeners.call(&Listener::pluginListChanged);
                if (onComplete)
                    onComplete(knownPluginList.getNumTypes());
            });
        });
    }

    std::unique_ptr<juce::AudioPluginInstance> PluginManager::loadPlugin(const juce::PluginDescription& desc,
                                                                         juce::String& errorMessage)
    {
        initialize();
        double sampleRate = 48000.0;
        int blockSize = 128;

        auto instance = formatManager.createPluginInstance(desc, sampleRate, blockSize, errorMessage);
        if (instance != nullptr)
        {
            instance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
        }
        return instance;
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

