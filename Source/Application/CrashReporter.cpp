#include "Application/CrashReporter.h"

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <shlobj.h>
 #include <cstdio>
 #include <cwchar>

namespace dsd
{
    namespace
    {
        void describeAddress(FILE* f, const void* address)
        {
            HMODULE module = nullptr;
            wchar_t path[MAX_PATH] = L"?";
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCWSTR>(address), &module) && module != nullptr)
            {
                GetModuleFileNameW(module, path, MAX_PATH);
                const wchar_t* name = std::wcsrchr(path, static_cast<wchar_t>(92)); // last backslash
                std::fwprintf(f, L"%ls+0x%llx", name != nullptr ? name + 1 : path,
                              static_cast<unsigned long long>(reinterpret_cast<const char*>(address) - reinterpret_cast<const char*>(module)));
            }
            else
            {
                std::fwprintf(f, L"0x%p", address);
            }
        }

        LONG WINAPI handler(EXCEPTION_POINTERS* info)
        {
            static volatile LONG entered = 0;
            if (InterlockedExchange(&entered, 1) != 0)
                return EXCEPTION_CONTINUE_SEARCH;

            wchar_t base[MAX_PATH];
            if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, base)))
                return EXCEPTION_CONTINUE_SEARCH;

            wchar_t dir[MAX_PATH];
            std::swprintf(dir, MAX_PATH, L"%ls/DSDMixer", base);
            CreateDirectoryW(dir, nullptr);
            std::swprintf(dir, MAX_PATH, L"%ls/DSDMixer/crashes", base);
            CreateDirectoryW(dir, nullptr);

            SYSTEMTIME t;
            GetLocalTime(&t);
            wchar_t file[MAX_PATH];
            std::swprintf(file, MAX_PATH, L"%ls/crash-%04d%02d%02d-%02d%02d%02d.txt", dir,
                          t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

            FILE* f = nullptr;
            if (_wfopen_s(&f, file, L"w") != 0 || f == nullptr)
                return EXCEPTION_CONTINUE_SEARCH;

            const auto* rec = info != nullptr ? info->ExceptionRecord : nullptr;
            std::fwprintf(f, L"DSD Mixer crash report\nthread %lu\nexception 0x%08lx at ",
                          GetCurrentThreadId(), rec != nullptr ? rec->ExceptionCode : 0ul);
            if (rec != nullptr)
                describeAddress(f, rec->ExceptionAddress);
            if (rec != nullptr && rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
                std::fwprintf(f, L"\n%ls address 0x%llx", rec->ExceptionInformation[0] == 0 ? L"read" : L"write",
                              static_cast<unsigned long long>(rec->ExceptionInformation[1]));
            std::fwprintf(f, L"\n\nstack:\n");

            void* frames[64];
            const USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);
            for (USHORT i = 0; i < count; ++i)
            {
                std::fwprintf(f, L"  %2u  ", static_cast<unsigned>(i));
                describeAddress(f, frames[i]);
                std::fwprintf(f, L"\n");
            }
            std::fclose(f);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    } // namespace

    void installCrashReporter()
    {
        SetUnhandledExceptionFilter(handler);
    }
} // namespace dsd

#else
namespace dsd { void installCrashReporter() {} }
#endif
