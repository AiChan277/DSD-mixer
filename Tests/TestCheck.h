#pragma once

// Non-aborting test assertions.
//
// The suites are built in Release, where the standard assert() compiles to
// nothing, so failures were silently ignored. CHECK() always evaluates its
// condition, reports every failure with file/line, and keeps running so one
// broken expectation does not hide the rest. main() returns
// dsd_test::exitCode() so a failing run exits non-zero.

#include <atomic>
#include <iostream>

namespace dsd_test
{
    inline std::atomic<int>& failureCount() noexcept
    {
        static std::atomic<int> count{0};
        return count;
    }

    inline void reportFailure(const char* expr, const char* file, int line)
    {
        failureCount().fetch_add(1, std::memory_order_relaxed);
        std::cout << "  [FAIL] " << expr << "  (" << file << ":" << line << ")" << std::endl;
    }

    inline int exitCode() noexcept
    {
        const int failures = failureCount().load(std::memory_order_relaxed);
        if (failures > 0)
            std::cout << "\n" << failures << " check(s) FAILED." << std::endl;
        return failures > 0 ? 1 : 0;
    }
} // namespace dsd_test

#define CHECK(cond) \
    do { if (!(cond)) ::dsd_test::reportFailure(#cond, __FILE__, __LINE__); } while (false)
