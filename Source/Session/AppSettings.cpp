#include "Session/AppSettings.h"

namespace dsd
{
    AppSettings& AppSettings::getInstance()
    {
        static AppSettings instance;
        return instance;
    }

    AppSettings::AppSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "DSDMixer";
        options.filenameSuffix      = ".settings";
        options.folderName          = "DSDMixer";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers    = false;
        options.ignoreCaseOfKeyNames = true;
        options.doNotSave           = false;
        options.millisecondsBeforeSaving = 1000;
        options.storageFormat       = juce::PropertiesFile::storeAsXML;

        appProperties.setStorageParameters(options);
    }

    AppSettings::~AppSettings()
    {
        saveIfNeeded();
    }

    bool AppSettings::getStartRemoteServerAuto() const
    {
        if (auto* props = appProperties.getUserSettings())
            return props->getBoolValue("startRemoteServerAuto", true);
        return true;
    }

    void AppSettings::setStartRemoteServerAuto(bool autoStart)
    {
        if (auto* props = appProperties.getUserSettings())
        {
            props->setValue("startRemoteServerAuto", autoStart);
            saveIfNeeded();
        }
    }

    int AppSettings::getRemoteServerPort() const
    {
        if (auto* props = appProperties.getUserSettings())
            return props->getIntValue("remoteServerPort", 9100);
        return 9100;
    }

    void AppSettings::setRemoteServerPort(int port)
    {
        if (auto* props = appProperties.getUserSettings())
        {
            props->setValue("remoteServerPort", port);
            saveIfNeeded();
        }
    }

    bool AppSettings::getMinimizeToTray() const
    {
        if (auto* props = appProperties.getUserSettings())
            return props->getBoolValue("minimizeToTray", true);
        return true;
    }

    void AppSettings::setMinimizeToTray(bool minimize)
    {
        if (auto* props = appProperties.getUserSettings())
        {
            props->setValue("minimizeToTray", minimize);
            saveIfNeeded();
        }
    }

    int AppSettings::getCaptureLatencyMode() const
    {
        if (auto* props = appProperties.getUserSettings())
            return props->getIntValue("captureLatencyMode", 1); // 1 = Low
        return 1;
    }

    void AppSettings::setCaptureLatencyMode(int mode)
    {
        if (auto* props = appProperties.getUserSettings())
        {
            props->setValue("captureLatencyMode", mode);
            saveIfNeeded();
        }
    }

    void AppSettings::saveIfNeeded()
    {
        appProperties.saveIfNeeded();
    }
} // namespace dsd
