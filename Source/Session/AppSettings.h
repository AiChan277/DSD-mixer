#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>

namespace dsd
{
    class AppSettings
    {
    public:
        static AppSettings& getInstance();

        bool getStartRemoteServerAuto() const;
        void setStartRemoteServerAuto(bool autoStart);

        int getRemoteServerPort() const;
        void setRemoteServerPort(int port);

        bool getMinimizeToTray() const;
        void setMinimizeToTray(bool minimize);

        int getCaptureLatencyMode() const;
        void setCaptureLatencyMode(int mode);

        void saveIfNeeded();

    private:
        AppSettings();
        ~AppSettings();

        mutable juce::ApplicationProperties appProperties;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
    };
} // namespace dsd
