#pragma once

#include <cstdint>

namespace dsd
{
    class ProcessCpuTracker
    {
    public:
        ProcessCpuTracker();
        void reset();
        float getCpuUsagePercent();

    private:
        uint64_t lastProcessTime{0};
        int64_t lastSampleTime{0};
        float lastCpuPercent{0.0f};
    };
} // namespace dsd
