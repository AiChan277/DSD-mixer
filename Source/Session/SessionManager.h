#pragma once

#include <juce_core/juce_core.h>
#include "Audio/AudioEngine.h"
#include "Audio/AudioDeviceManager.h"

namespace dsd
{
    class SessionManager
    {
    public:
        SessionManager() = default;
        ~SessionManager() = default;

        static bool saveSessionToFile(const juce::File& file,
                                      const AudioEngine& engine,
                                      const AudioDeviceManager& devManager);

        static bool loadSessionFromFile(const juce::File& file,
                                        AudioEngine& engine,
                                        AudioDeviceManager& devManager);
    };
} // namespace dsd
