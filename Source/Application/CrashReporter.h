#pragma once

namespace dsd
{
    // Installs a last-chance exception handler that writes a small crash report
    // (exception code, faulting address and a module-relative stack trace) to
    // %APPDATA%\DSDMixer\crashes\crash-<timestamp>.txt. Addresses can be mapped
    // back to functions with the linker map file produced by the build.
    void installCrashReporter();
}
