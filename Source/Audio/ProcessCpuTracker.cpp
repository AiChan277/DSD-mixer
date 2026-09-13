#include "Audio/ProcessCpuTracker.h"
#include <juce_core/juce_core.h>
#include <algorithm>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace dsd
{
    ProcessCpuTracker::ProcessCpuTracker()
    {
        reset();
    }

    void ProcessCpuTracker::reset()
    {
#if JUCE_WINDOWS
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER k, u;
            k.LowPart = kernelTime.dwLowDateTime;
            k.HighPart = kernelTime.dwHighDateTime;
            u.LowPart = userTime.dwLowDateTime;
            u.HighPart = userTime.dwHighDateTime;
            lastProcessTime = k.QuadPart + u.QuadPart;
        }
        lastSampleTime = juce::Time::getHighResolutionTicks();
#endif
    }

    float ProcessCpuTracker::getCpuUsagePercent()
    {
#if JUCE_WINDOWS
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER k, u;
            k.LowPart = kernelTime.dwLowDateTime;
            k.HighPart = kernelTime.dwHighDateTime;
            u.LowPart = userTime.dwLowDateTime;
            u.HighPart = userTime.dwHighDateTime;
            const uint64_t currentProcessTime = k.QuadPart + u.QuadPart;
            const int64_t currentSampleTime = juce::Time::getHighResolutionTicks();

            const double elapsedWallSec = juce::Time::highResolutionTicksToSeconds(currentSampleTime - lastSampleTime);
            if (elapsedWallSec >= 0.2) // update every ~200ms
            {
                const uint64_t deltaProcessTime100ns = currentProcessTime - lastProcessTime;
                const double processCpuSec = static_cast<double>(deltaProcessTime100ns) * 1e-7;
                const int numLogicalCores = std::max(1, juce::SystemStats::getNumCpus());

                const double usage = (processCpuSec / (elapsedWallSec * numLogicalCores)) * 100.0;
                lastCpuPercent = std::clamp(static_cast<float>(usage), 0.0f, 100.0f);

                lastProcessTime = currentProcessTime;
                lastSampleTime = currentSampleTime;
            }
        }
        return lastCpuPercent;
#else
        return 0.0f;
#endif
    }
} // namespace dsd
