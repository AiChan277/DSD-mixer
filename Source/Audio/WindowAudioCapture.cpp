#include "Audio/WindowAudioCapture.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <audiopolicy.h>
#include <avrt.h>
#include <timeapi.h>
#include <psapi.h>
#include <propvarutil.h>
#include <dwmapi.h>
#include <iostream>
#include <unordered_set>
#include <cmath>

#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")

#ifndef AUDCLNT_STREAMOPTIONS_RAW
#define AUDCLNT_STREAMOPTIONS_RAW 0x1
#endif

namespace dsd
{
    // =========================================================================
    // COM Async Activation Handler for Process Loopback
    // =========================================================================
    class ActivateAudioInterfaceCompletionHandler
        : public IActivateAudioInterfaceCompletionHandler,
          public IAgileObject
    {
    public:
        ActivateAudioInterfaceCompletionHandler()
            : refCount(1), completedEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr))
        {
            CoCreateFreeThreadedMarshaler(static_cast<IActivateAudioInterfaceCompletionHandler*>(this), &ftm);
        }

        ~ActivateAudioInterfaceCompletionHandler()
        {
            if (ftm != nullptr)
            {
                ftm->Release();
                ftm = nullptr;
            }
            if (unk != nullptr)
            {
                unk->Release();
                unk = nullptr;
            }
            if (completedEvent != nullptr)
            {
                CloseHandle(completedEvent);
                completedEvent = nullptr;
            }
        }

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (ppv == nullptr) return E_POINTER;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler))
            {
                *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
                AddRef();
                return S_OK;
            }
            if (riid == __uuidof(IAgileObject))
            {
                *ppv = static_cast<IAgileObject*>(this);
                AddRef();
                return S_OK;
            }
            if (riid == __uuidof(IMarshal) && ftm != nullptr)
            {
                return ftm->QueryInterface(riid, ppv);
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override
        {
            return InterlockedIncrement(&refCount);
        }

        STDMETHODIMP_(ULONG) Release() override
        {
            ULONG val = InterlockedDecrement(&refCount);
            if (val == 0)
                delete this;
            return val;
        }

        STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override
        {
            if (op != nullptr)
            {
                op->GetActivateResult(&hrActivateResult, &unk);
            }
            SetEvent(completedEvent);
            return S_OK;
        }

        HANDLE completedEvent{nullptr};
        HRESULT hrActivateResult{E_FAIL};
        IUnknown* unk{nullptr};
        IUnknown* ftm{nullptr};

    private:
        ULONG refCount;
    };

    // =========================================================================
    // WindowAudioCapture
    // =========================================================================
    WindowAudioCapture::WindowAudioCapture(juce::uint32 targetPid, const juce::String& processName)
        : pid(targetPid), procName(processName)
    {
    }

    WindowAudioCapture::~WindowAudioCapture()
    {
        releaseResources();
    }

    int WindowAudioCapture::calculateTargetOccupancy(CaptureLatencyMode mode, int blockSize, double sampleRate) const
    {
        const double rateScale = (sampleRate > 1000.0) ? (sampleRate / 48000.0) : 1.0;
        switch (mode)
        {
            case CaptureLatencyMode::UltraLow:
                // Trough cushion ~2.67 ms (128 frames) - lowest stable empirical occupancy without XRUNs
                return static_cast<int>(std::round(std::max(128.0 * rateScale, static_cast<double>(blockSize))));
            case CaptureLatencyMode::Low:
                // Trough cushion ~4.0 ms (192 frames) - optimal balance of low latency & jitter immunity
                return static_cast<int>(std::round(std::max(192.0 * rateScale, static_cast<double>(blockSize) + 64.0 * rateScale)));
            case CaptureLatencyMode::Standard:
            default:
                // Trough cushion ~8.0 ms (384 frames) - safe jitter tolerance
                return static_cast<int>(std::round(std::max(384.0 * rateScale, static_cast<double>(blockSize) + 256.0 * rateScale)));
        }
    }

    void WindowAudioCapture::setLatencyMode(CaptureLatencyMode mode)
    {
        latencyMode.store(mode, std::memory_order_relaxed);
        const int targetOcc = calculateTargetOccupancy(mode, currentMaxBlockSize, targetSampleRate);
        ringBuffer.setTargetOccupancy(targetOcc);
    }

    void WindowAudioCapture::setGlobalLatencyMode(CaptureLatencyMode mode)
    {
        globalLatencyMode.store(mode, std::memory_order_relaxed);
    }

    void WindowAudioCapture::prepare(double sampleRate, int maxBlockSize)
    {
        targetSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        currentMaxBlockSize = (maxBlockSize > 0) ? maxBlockSize : 256;
        latencyMode.store(globalLatencyMode.load(std::memory_order_relaxed), std::memory_order_relaxed);
        const int targetOcc = calculateTargetOccupancy(latencyMode.load(std::memory_order_relaxed), currentMaxBlockSize, targetSampleRate);
        ringBuffer.setTargetOccupancy(targetOcc);

        shouldStop.store(true);
        if (captureThread.joinable())
            captureThread.join();

        shouldStop.store(false);
        isRunning.store(false);
        ringBuffer.reset();
        captureThread = std::thread(&WindowAudioCapture::threadLoop, this);
    }

    void WindowAudioCapture::releaseResources()
    {
        shouldStop.store(true);
        if (captureThread.joinable())
            captureThread.join();

        isRunning.store(false);
        ringBuffer.reset();
    }

    void WindowAudioCapture::readBlock(juce::AudioBuffer<float>& targetBuffer,
                                       const juce::AudioBuffer<float>& /*deviceInputBuffer*/,
                                       int numSamples)
    {
        if (!isRunning.load(std::memory_order_relaxed) || numSamples <= 0)
        {
            for (int ch = 0; ch < targetBuffer.getNumChannels(); ++ch)
                targetBuffer.clear(ch, 0, numSamples);
            return;
        }

        ringBuffer.read(targetBuffer.getArrayOfWritePointers(), targetBuffer.getNumChannels(), numSamples, targetSampleRate);
    }

    CaptureDiagnostics WindowAudioCapture::getDiagnostics() const
    {
        CaptureDiagnostics diag;
        diag.processId = pid;
        diag.appName = procName;
        diag.isCapturing = isRunning.load(std::memory_order_relaxed);
        diag.captureSampleRate = captureSampleRate;
        diag.captureChannels = captureChannels;
        diag.bitsPerSample = bitsPerSample;
        diag.isFloat = isNativeFloat;

        diag.latencyMode = latencyMode.load(std::memory_order_relaxed);
        switch (diag.latencyMode)
        {
            case CaptureLatencyMode::UltraLow: diag.latencyModeName = "Ultra-Low (2.7ms FIFO)"; break;
            case CaptureLatencyMode::Low:      diag.latencyModeName = "Low (4.0ms FIFO)"; break;
            case CaptureLatencyMode::Standard: diag.latencyModeName = "Standard (8.0ms FIFO)"; break;
        }

        diag.wasapiBufferFrames = wasapiBufferFrames.load(std::memory_order_relaxed);
        diag.wasapiBufferMs = (captureSampleRate > 0) ? (static_cast<double>(diag.wasapiBufferFrames) / captureSampleRate * 1000.0) : 0.0;
        diag.wasapiPeriodMs = wasapiPeriodMs.load(std::memory_order_relaxed);

        diag.ringBufferCapacity = ringBuffer.getCapacity();
        diag.ringBufferOccupancy = ringBuffer.getNumReady();
        diag.ringBufferOccupancyMs = (targetSampleRate > 0) ? (static_cast<double>(diag.ringBufferOccupancy) / targetSampleRate * 1000.0) : 0.0;
        diag.targetOccupancy = ringBuffer.getTargetOccupancy();
        diag.targetOccupancyMs = (targetSampleRate > 0) ? (static_cast<double>(diag.targetOccupancy) / targetSampleRate * 1000.0) : 0.0;

        diag.smoothedOccupancy = ringBuffer.getSmoothedOccupancy();
        diag.clockDriftPpm = ringBuffer.getClockDriftPpm();
        diag.totalCaptureFrames = ringBuffer.getTotalFramesWritten();
        diag.totalEngineFrames = ringBuffer.getTotalFramesRead();

        diag.underrunCount = ringBuffer.getUnderrunCount();
        diag.overrunCount = ringBuffer.getOverrunCount();
        diag.discontinuityCount = discontinuityCount.load(std::memory_order_relaxed);
        diag.driftCorrections = ringBuffer.getDriftCorrections();
        diag.burstFlushedFrames = ringBuffer.getBurstFlushedFrames();

        // Exact measured QPC hardware packet age (ground truth)
        diag.hardwarePacketAgeMs = latestHardwarePacketAgeMs.load(std::memory_order_relaxed);
        diag.estimatedCaptureLatencyMs = diag.hardwarePacketAgeMs + diag.ringBufferOccupancyMs;
        return diag;
    }

    bool WindowAudioCapture::activateProcessLoopback(IAudioClient** outClient)
    {
        if (outClient == nullptr || pid == 0)
        {
            DBG("[WindowAudioCapture] activateProcessLoopback invalid args: pid=" << (int)pid);
            return false;
        }

        *outClient = nullptr;

        AUDIOCLIENT_ACTIVATION_PARAMS params = {};
        params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        params.ProcessLoopbackParams.TargetProcessId = pid;
        params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

        PROPVARIANT activateParams = {};
        activateParams.vt = VT_BLOB;
        activateParams.blob.cbSize = sizeof(params);
        activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

        auto* handler = new ActivateAudioInterfaceCompletionHandler();
        IActivateAudioInterfaceAsyncOperation* asyncOp = nullptr;

        HRESULT hr = ActivateAudioInterfaceAsync(
            VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
            __uuidof(IAudioClient),
            &activateParams,
            handler,
            &asyncOp);

        if (FAILED(hr))
        {
            DBG("[WindowAudioCapture] ActivateAudioInterfaceAsync failed: 0x" << juce::String::toHexString((juce::uint32)hr));
            handler->Release();
            return false;
        }

        // Wait for async activation callback
        DWORD waitRes = WaitForSingleObject(handler->completedEvent, 3000);
        if (waitRes != WAIT_OBJECT_0)
        {
            DBG("[WindowAudioCapture] WaitForSingleObject timeout waitRes=" << (int)waitRes);
            if (asyncOp != nullptr)
                asyncOp->Release();
            handler->Release();
            return false;
        }

        DBG("[WindowAudioCapture] Activation completed! hrActivateResult=0x" << juce::String::toHexString((juce::uint32)handler->hrActivateResult));

        bool success = false;
        if (SUCCEEDED(handler->hrActivateResult) && handler->unk != nullptr)
        {
            hr = handler->unk->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void**>(outClient));
            if (SUCCEEDED(hr) && *outClient != nullptr)
                success = true;
        }

        if (asyncOp != nullptr)
            asyncOp->Release();

        handler->Release();
        return success;
    }

    struct MmcssScope
    {
        MmcssScope()
        {
            timeBeginPeriod(1);
            DWORD taskIndex = 0;
            hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
            if (hTask != nullptr)
                AvSetMmThreadPriority(hTask, AVRT_PRIORITY_CRITICAL);
            else
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        }
        ~MmcssScope()
        {
            if (hTask != nullptr)
                AvRevertMmThreadCharacteristics(hTask);
            timeEndPeriod(1);
        }
        HANDLE hTask{nullptr};
    };

    void WindowAudioCapture::threadLoop()
    {
        MmcssScope mmcss;
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        IAudioClient* client = nullptr;
        if (!activateProcessLoopback(&client) || client == nullptr)
        {
            DBG("[WindowAudioCapture] Failed to activate process loopback for PID: " << (int)pid);
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        // 0. Set RAW stream properties on IAudioClient2 if available to bypass Windows DSP/APOs
        IAudioClient2* client2 = nullptr;
        if (SUCCEEDED(client->QueryInterface(__uuidof(IAudioClient2), reinterpret_cast<void**>(&client2))) && client2 != nullptr)
        {
            AudioClientProperties prop = {};
            prop.cbSize = sizeof(AudioClientProperties);
            prop.bIsOffload = FALSE;
            prop.eCategory = AudioCategory_Media;
            prop.Options = static_cast<AUDCLNT_STREAMOPTIONS>(AUDCLNT_STREAMOPTIONS_RAW);
            HRESULT hrProp = client2->SetClientProperties(&prop);
            DBG("[WindowAudioCapture] SetClientProperties(RAW) result: 0x" << juce::String::toHexString((juce::uint32)hrProp));
            client2->Release();
        }

        // ---------------------------------------------------------------------
        // Format Negotiation for Process Loopback
        // On VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, client->GetMixFormat() returns
        // E_NOTIMPL. We prepare candidate formats and initialize with auto-convert.
        // ---------------------------------------------------------------------

        // 1. Primary candidate: 48 kHz 32-bit Float Stereo (WASAPI standard native float)
        WAVEFORMATEXTENSIBLE wfxFloat48 = {};
        wfxFloat48.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfxFloat48.Format.nChannels = 2;
        wfxFloat48.Format.nSamplesPerSec = 48000;
        wfxFloat48.Format.wBitsPerSample = 32;
        wfxFloat48.Format.nBlockAlign = (wfxFloat48.Format.nChannels * wfxFloat48.Format.wBitsPerSample) / 8; // 8
        wfxFloat48.Format.nAvgBytesPerSec = wfxFloat48.Format.nSamplesPerSec * wfxFloat48.Format.nBlockAlign; // 384000
        wfxFloat48.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfxFloat48.Samples.wValidBitsPerSample = 32;
        wfxFloat48.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
        wfxFloat48.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        // 2. Second candidate: Default audio render endpoint mix format
        WAVEFORMATEX* pDevWfx = nullptr;
        IMMDeviceEnumerator* enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) && enumerator != nullptr)
        {
            IMMDevice* defaultDevice = nullptr;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice)) && defaultDevice != nullptr)
            {
                IAudioClient* defaultClient = nullptr;
                if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&defaultClient))) && defaultClient != nullptr)
                {
                    defaultClient->GetMixFormat(&pDevWfx);
                    defaultClient->Release();
                }
                defaultDevice->Release();
            }
            enumerator->Release();
        }

        // 3. Third candidate: 48 kHz 16-bit PCM Stereo
        WAVEFORMATEX wfxPcm16_48 = {};
        wfxPcm16_48.wFormatTag = WAVE_FORMAT_PCM;
        wfxPcm16_48.nChannels = 2;
        wfxPcm16_48.nSamplesPerSec = 48000;
        wfxPcm16_48.wBitsPerSample = 16;
        wfxPcm16_48.nBlockAlign = 4;
        wfxPcm16_48.nAvgBytesPerSec = 48000 * 4;
        wfxPcm16_48.cbSize = 0;

        // 4. Fourth candidate: 44.1 kHz 16-bit PCM Stereo (Microsoft sample default)
        WAVEFORMATEX wfxPcm16_44 = {};
        wfxPcm16_44.wFormatTag = WAVE_FORMAT_PCM;
        wfxPcm16_44.nChannels = 2;
        wfxPcm16_44.nSamplesPerSec = 44100;
        wfxPcm16_44.wBitsPerSample = 16;
        wfxPcm16_44.nBlockAlign = 4;
        wfxPcm16_44.nAvgBytesPerSec = 44100 * 4;
        wfxPcm16_44.cbSize = 0;

        struct FormatCandidate
        {
            const WAVEFORMATEX* pwfx;
            bool isFloat;
        };

        std::vector<FormatCandidate> candidates;
        // Prioritize native 32-bit Float (48 kHz & device mix format) for 1:1 bit-exact capture with zero conversion
        candidates.push_back({ reinterpret_cast<const WAVEFORMATEX*>(&wfxFloat48), true });
        if (pDevWfx != nullptr)
        {
            bool devIsFloat = (pDevWfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                               (pDevWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                                 reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pDevWfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
            candidates.push_back({ pDevWfx, devIsFloat });
        }
        // Fallbacks if native float is not accepted
        candidates.push_back({ &wfxPcm16_48, false });
        candidates.push_back({ &wfxPcm16_44, false });

        bool initialized = false;
        WAVEFORMATEXTENSIBLE activeFormatExt = {};
        bool activeIsFloat = false;

        const DWORD flagSets[] = {
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
        };

        // Check device period limits to negotiate minimum latency
        REFERENCE_TIME defaultPeriod = 0, minPeriod = 0;
        if (SUCCEEDED(client->GetDevicePeriod(&defaultPeriod, &minPeriod)) && defaultPeriod > 0)
        {
            wasapiPeriodMs.store(static_cast<double>(defaultPeriod) / 10000.0, std::memory_order_relaxed);
        }
        else
        {
            wasapiPeriodMs.store(10.0, std::memory_order_relaxed);
        }

        // Try IAudioClient3 for true low-latency engine period if supported
        IAudioClient3* client3 = nullptr;
        if (SUCCEEDED(client->QueryInterface(__uuidof(IAudioClient3), reinterpret_cast<void**>(&client3))) && client3 != nullptr)
        {
            for (const auto& candidate : candidates)
            {
                for (DWORD flags : flagSets)
                {
                    UINT32 defP = 0, fundP = 0, minP = 0, maxP = 0;
                    if (SUCCEEDED(client3->GetSharedModeEnginePeriod(candidate.pwfx, &defP, &fundP, &minP, &maxP)) && minP > 0)
                    {
                        if (SUCCEEDED(client3->InitializeSharedAudioStream(flags, minP, candidate.pwfx, nullptr)))
                        {
                            if (candidate.pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                                memcpy(&activeFormatExt, candidate.pwfx, sizeof(WAVEFORMATEXTENSIBLE));
                            else
                            {
                                memcpy(&activeFormatExt.Format, candidate.pwfx, sizeof(WAVEFORMATEX));
                                activeFormatExt.Format.cbSize = 0;
                            }
                            activeIsFloat = candidate.isFloat;
                            initialized = true;
                            DBG("[WindowAudioCapture] IAudioClient3::InitializeSharedAudioStream SUCCESS with minPeriod: " << (int)minP << " frames!");
                            break;
                        }
                    }
                }
                if (initialized) break;
            }
            client3->Release();
        }

        // Fallback to IAudioClient::Initialize with aggressive low buffer durations
        if (!initialized)
        {
            std::vector<REFERENCE_TIME> bufferDurations;
            if (minPeriod > 0)
                bufferDurations.push_back(minPeriod);
            bufferDurations.push_back(20000);  // 2.0 ms
            bufferDurations.push_back(30000);  // 3.0 ms
            bufferDurations.push_back(50000);  // 5.0 ms
            bufferDurations.push_back(100000); // 10.0 ms
            bufferDurations.push_back(0);      // Engine automatic default
            bufferDurations.push_back(2000000);// Safe fallback

            for (const auto& candidate : candidates)
            {
                for (DWORD flags : flagSets)
                {
                    for (REFERENCE_TIME hnsBuffer : bufferDurations)
                    {
                        HRESULT hrInit = client->Initialize(
                            AUDCLNT_SHAREMODE_SHARED,
                            flags,
                            hnsBuffer,
                            0,
                            candidate.pwfx,
                            nullptr);

                        if (SUCCEEDED(hrInit))
                        {
                            if (candidate.pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                            {
                                memcpy(&activeFormatExt, candidate.pwfx, sizeof(WAVEFORMATEXTENSIBLE));
                            }
                            else
                            {
                                memcpy(&activeFormatExt.Format, candidate.pwfx, sizeof(WAVEFORMATEX));
                                activeFormatExt.Format.cbSize = 0;
                            }
                            activeIsFloat = candidate.isFloat;
                            initialized = true;
                            break;
                        }
                    }
                    if (initialized) break;
                }
                if (initialized) break;
            }
        }

        if (pDevWfx != nullptr)
        {
            CoTaskMemFree(pDevWfx);
            pDevWfx = nullptr;
        }

        if (!initialized)
        {
            DBG("[WindowAudioCapture] client->Initialize failed for all candidate formats on PID: " << (int)pid);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        DBG("[WindowAudioCapture] client->Initialize SUCCESS for PID " << (int)pid << " (" << procName << ")");

        // Query actual allocated buffer size and device period
        UINT32 allocatedBufferFrames = 0;
        if (SUCCEEDED(client->GetBufferSize(&allocatedBufferFrames)))
        {
            wasapiBufferFrames.store(allocatedBufferFrames, std::memory_order_relaxed);
        }

        if (SUCCEEDED(client->GetDevicePeriod(&defaultPeriod, &minPeriod)) && defaultPeriod > 0)
        {
            wasapiPeriodMs.store(static_cast<double>(defaultPeriod) / 10000.0, std::memory_order_relaxed);
        }
        else
        {
            wasapiPeriodMs.store(10.0, std::memory_order_relaxed);
        }

        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        HRESULT hr = client->SetEventHandle(hEvent);
        if (FAILED(hr))
        {
            DBG("[WindowAudioCapture] SetEventHandle failed: 0x" << juce::String::toHexString((juce::uint32)hr));
            CloseHandle(hEvent);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        IAudioCaptureClient* captureClient = nullptr;
        hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));
        if (FAILED(hr) || captureClient == nullptr)
        {
            DBG("[WindowAudioCapture] GetService(IAudioCaptureClient) failed: 0x" << juce::String::toHexString((juce::uint32)hr));
            CloseHandle(hEvent);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        hr = client->Start();
        if (FAILED(hr))
        {
            DBG("[WindowAudioCapture] client->Start() failed: 0x" << juce::String::toHexString((juce::uint32)hr));
            captureClient->Release();
            CloseHandle(hEvent);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        captureSampleRate = activeFormatExt.Format.nSamplesPerSec > 0 ? static_cast<double>(activeFormatExt.Format.nSamplesPerSec) : 48000.0;
        captureChannels = activeFormatExt.Format.nChannels > 0 ? activeFormatExt.Format.nChannels : 2;
        bitsPerSample = activeFormatExt.Format.wBitsPerSample;
        isNativeFloat = activeIsFloat;

        isRunning.store(true);
        DBG("[WindowAudioCapture] Started process loopback: " << procName
            << " (PID: " << (int)pid << ") at " << captureSampleRate << " Hz, " << captureChannels
            << " ch, " << bitsPerSample << " bits (" << (activeIsFloat ? "Float" : "PCM")
            << "), HW buffer: " << allocatedBufferFrames << " frames, period: "
            << wasapiPeriodMs.load() << " ms");

        interpolator[0].reset();
        interpolator[1].reset();

        juce::AudioBuffer<float> tempCaptureBuffer(2, 4096);
        resampleBuffer.setSize(2, 4096);

        while (!shouldStop.load())
        {
            DWORD waitRes = WaitForSingleObject(hEvent, 20);
            if (shouldStop.load())
                break;

            UINT32 packetLength = 0;
            while (SUCCEEDED(captureClient->GetNextPacketSize(&packetLength)) && packetLength > 0 && !shouldStop.load())
            {
                BYTE* pData = nullptr;
                UINT32 numFramesRead = 0;
                DWORD flags = 0;
                UINT64 devPos = 0;
                UINT64 qpcPos = 0;

                hr = captureClient->GetBuffer(&pData, &numFramesRead, &flags, &devPos, &qpcPos);
                if (FAILED(hr))
                    break;

                if (qpcPos > 0)
                {
                    LARGE_INTEGER qpcNow, qpcFreq;
                    QueryPerformanceCounter(&qpcNow);
                    QueryPerformanceFrequency(&qpcFreq);
                    if (qpcFreq.QuadPart > 0)
                    {
                        double ageMs = static_cast<double>(qpcNow.QuadPart - static_cast<LONGLONG>(qpcPos)) * 1000.0 / qpcFreq.QuadPart;
                        if (ageMs > 0.0 && ageMs < 200.0)
                        {
                            double prevAge = latestHardwarePacketAgeMs.load(std::memory_order_relaxed);
                            latestHardwarePacketAgeMs.store(prevAge <= 0.1 ? ageMs : (0.95 * prevAge + 0.05 * ageMs), std::memory_order_relaxed);
                        }
                    }
                }

                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
                {
                    discontinuityCount.fetch_add(1, std::memory_order_relaxed);
                }

                if (numFramesRead > 0 && pData != nullptr)
                {
                    if (tempCaptureBuffer.getNumSamples() < static_cast<int>(numFramesRead))
                        tempCaptureBuffer.setSize(2, numFramesRead, false, false, true);

                    float* leftDst = tempCaptureBuffer.getWritePointer(0);
                    float* rightDst = tempCaptureBuffer.getWritePointer(1);

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    {
                        tempCaptureBuffer.clear(0, numFramesRead);
                    }
                    else if (activeIsFloat && bitsPerSample == 32)
                    {
                        const float* floatSrc = reinterpret_cast<const float*>(pData);
                        if (captureChannels >= 2)
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                leftDst[i] = floatSrc[i * captureChannels];
                                rightDst[i] = floatSrc[i * captureChannels + 1];
                            }
                        }
                        else
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                float s = floatSrc[i];
                                leftDst[i] = s;
                                rightDst[i] = s;
                            }
                        }
                    }
                    else if (bitsPerSample == 16)
                    {
                        const int16_t* intSrc = reinterpret_cast<const int16_t*>(pData);
                        constexpr float inv32768 = 1.0f / 32768.0f;
                        if (captureChannels >= 2)
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                leftDst[i] = static_cast<float>(intSrc[i * captureChannels]) * inv32768;
                                rightDst[i] = static_cast<float>(intSrc[i * captureChannels + 1]) * inv32768;
                            }
                        }
                        else
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                float s = static_cast<float>(intSrc[i]) * inv32768;
                                leftDst[i] = s;
                                rightDst[i] = s;
                            }
                        }
                    }
                    else if (bitsPerSample == 24)
                    {
                        const uint8_t* byteSrc = reinterpret_cast<const uint8_t*>(pData);
                        constexpr float inv8388608 = 1.0f / 8388608.0f;
                        const int stride = 3 * captureChannels;
                        for (UINT32 i = 0; i < numFramesRead; ++i)
                        {
                            const uint8_t* pFrame = byteSrc + i * stride;
                            int32_t s0 = static_cast<int32_t>((pFrame[0] << 8) | (pFrame[1] << 16) | (pFrame[2] << 24)) >> 8;
                            int32_t s1 = (captureChannels >= 2)
                                ? (static_cast<int32_t>((pFrame[3] << 8) | (pFrame[4] << 16) | (pFrame[5] << 24)) >> 8)
                                : s0;

                            leftDst[i] = static_cast<float>(s0) * inv8388608;
                            rightDst[i] = static_cast<float>(s1) * inv8388608;
                        }
                    }
                    else if (!activeIsFloat && bitsPerSample == 32)
                    {
                        const int32_t* int32Src = reinterpret_cast<const int32_t*>(pData);
                        constexpr float inv2147483648 = 1.0f / 2147483648.0f;
                        if (captureChannels >= 2)
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                leftDst[i] = static_cast<float>(int32Src[i * captureChannels]) * inv2147483648;
                                rightDst[i] = static_cast<float>(int32Src[i * captureChannels + 1]) * inv2147483648;
                            }
                        }
                        else
                        {
                            for (UINT32 i = 0; i < numFramesRead; ++i)
                            {
                                float s = static_cast<float>(int32Src[i]) * inv2147483648;
                                leftDst[i] = s;
                                rightDst[i] = s;
                            }
                        }
                    }
                    else
                    {
                        tempCaptureBuffer.clear(0, numFramesRead);
                    }

                    // Resample to engine target rate if needed
                    if (std::abs(captureSampleRate - targetSampleRate) < 1.0)
                    {
                        ringBuffer.write(tempCaptureBuffer.getArrayOfReadPointers(), 2, numFramesRead);
                    }
                    else
                    {
                        const double speedRatio = captureSampleRate / targetSampleRate;
                        const int numProduced = static_cast<int>(std::round(static_cast<double>(numFramesRead) / speedRatio));
                        if (numProduced > 0)
                        {
                            if (resampleBuffer.getNumSamples() < numProduced)
                                resampleBuffer.setSize(2, numProduced, false, false, true);

                            for (int ch = 0; ch < 2; ++ch)
                            {
                                interpolator[ch].process(speedRatio, tempCaptureBuffer.getReadPointer(ch),
                                                         resampleBuffer.getWritePointer(ch), numProduced, numFramesRead, 0);
                            }
                            ringBuffer.write(resampleBuffer.getArrayOfReadPointers(), 2, numProduced);
                        }
                    }
                }

                captureClient->ReleaseBuffer(numFramesRead);
            }
        }

        client->Stop();
        captureClient->Release();
        client->Release();
        CloseHandle(hEvent);

        isRunning.store(false);

        if (SUCCEEDED(hrCo))
            CoUninitialize();
    }

    // =========================================================================
    // Application Enumerator
    // =========================================================================
    struct WindowEnumData
    {
        std::vector<RunningAppInfo> apps;
        std::unordered_set<DWORD> seenPids;
        int total{0};
        int visible{0};
        int withTitle{0};
        int stylePass{0};
        int uncloaked{0};
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
    {
        auto* data = reinterpret_cast<WindowEnumData*>(lParam);
        data->total++;

        if (!IsWindowVisible(hWnd))
            return TRUE;
        data->visible++;

        int titleLen = GetWindowTextLengthW(hWnd);
        if (titleLen <= 0)
            return TRUE;
        data->withTitle++;

        // Skip windows without top-level visible styles
        LONG style = GetWindowLongW(hWnd, GWL_STYLE);
        if (!(style & WS_VISIBLE) || (style & WS_CHILD))
            return TRUE;
        data->stylePass++;

        // Check if window is cloaked (hidden virtual desktop or suspended UWP tile)
        int cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
            return TRUE;
        data->uncloaked++;

        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid == 0 || pid == GetCurrentProcessId())
            return TRUE;

        std::vector<wchar_t> titleBuf(titleLen + 1);
        GetWindowTextW(hWnd, titleBuf.data(), titleLen + 1);
        juce::String title(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(titleBuf.data())));

        // Query process executable name
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        juce::String appName = "App";
        if (hProcess != nullptr)
        {
            wchar_t pathBuf[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &size))
            {
                juce::String fullPath(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(pathBuf)));
                appName = juce::File(fullPath).getFileNameWithoutExtension();
            }
            CloseHandle(hProcess);
        }

        // Handle UWP apps hosted by ApplicationFrameHost (e.g. Spotify Store App, Edge, Netflix)
        if (appName.equalsIgnoreCase("ApplicationFrameHost"))
        {
            DWORD realPid = 0;
            EnumChildWindows(hWnd, [](HWND childHwnd, LPARAM lP) -> BOOL
            {
                DWORD childPid = 0;
                GetWindowThreadProcessId(childHwnd, &childPid);
                if (childPid != 0 && childPid != GetCurrentProcessId())
                {
                    HANDLE hChildProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, childPid);
                    if (hChildProc != nullptr)
                    {
                        wchar_t cPath[MAX_PATH];
                        DWORD cSize = MAX_PATH;
                        if (QueryFullProcessImageNameW(hChildProc, 0, cPath, &cSize))
                        {
                            juce::String cName = juce::File(juce::String(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(cPath)))).getFileNameWithoutExtension();
                            if (!cName.equalsIgnoreCase("ApplicationFrameHost"))
                            {
                                *reinterpret_cast<DWORD*>(lP) = childPid;
                                CloseHandle(hChildProc);
                                return FALSE; // Found real child UWP process PID, stop search
                            }
                        }
                        CloseHandle(hChildProc);
                    }
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&realPid));

            if (realPid != 0)
            {
                pid = realPid;
                HANDLE hReal = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (hReal != nullptr)
                {
                    wchar_t pathBuf[MAX_PATH];
                    DWORD size = MAX_PATH;
                    if (QueryFullProcessImageNameW(hReal, 0, pathBuf, &size))
                    {
                        juce::String fullPath(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(pathBuf)));
                        appName = juce::File(fullPath).getFileNameWithoutExtension();
                    }
                    CloseHandle(hReal);
                }
            }
        }

        // Filter system utilities that don't output user audio
        if (title == "Program Manager" || title == "Settings" || title == "Windows Input Experience" ||
            appName.equalsIgnoreCase("ApplicationFrameHost") || appName.equalsIgnoreCase("SearchApp") ||
            appName.equalsIgnoreCase("StartMenuExperienceHost") || appName.equalsIgnoreCase("ShellExperienceHost"))
        {
            return TRUE;
        }

        if (data->seenPids.count(pid) > 0)
        {
            // If already discovered via audio sessions, update with real window title
            for (auto& app : data->apps)
            {
                if (app.processId == pid && app.windowTitle == "Audio Stream")
                {
                    app.windowTitle = title;
                    break;
                }
            }
            return TRUE;
        }

        data->seenPids.insert(pid);
        data->apps.push_back({ pid, appName, title });
        return TRUE;
    }

    std::vector<RunningAppInfo> WindowAudioCapture::getRunningApplications()
    {
        WindowEnumData data;

        // 1. WASAPI Active Audio Sessions Enumerator
        // Discovers all applications currently connected to Windows audio playback (Spotify, Discord, Chrome, Games, etc.)
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        IMMDeviceEnumerator* pEnum = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnum))) && pEnum != nullptr)
        {
            IMMDevice* pDev = nullptr;
            if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDev)) && pDev != nullptr)
            {
                IAudioSessionManager2* pMgr = nullptr;
                if (SUCCEEDED(pDev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pMgr))) && pMgr != nullptr)
                {
                    IAudioSessionEnumerator* pSessionEnum = nullptr;
                    if (SUCCEEDED(pMgr->GetSessionEnumerator(&pSessionEnum)) && pSessionEnum != nullptr)
                    {
                        int count = 0;
                        pSessionEnum->GetCount(&count);
                        for (int i = 0; i < count; ++i)
                        {
                            IAudioSessionControl* pCtrl = nullptr;
                            if (SUCCEEDED(pSessionEnum->GetSession(i, &pCtrl)) && pCtrl != nullptr)
                            {
                                IAudioSessionControl2* pCtrl2 = nullptr;
                                if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&pCtrl2))) && pCtrl2 != nullptr)
                                {
                                    DWORD pid = 0;
                                    if (SUCCEEDED(pCtrl2->GetProcessId(&pid)) && pid != 0 && pid != GetCurrentProcessId())
                                    {
                                        if (data.seenPids.count(pid) == 0)
                                        {
                                            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                                            if (hProc != nullptr)
                                            {
                                                wchar_t pBuf[MAX_PATH];
                                                DWORD pSize = MAX_PATH;
                                                if (QueryFullProcessImageNameW(hProc, 0, pBuf, &pSize))
                                                {
                                                    juce::String fullPath(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(pBuf)));
                                                    juce::String appName = juce::File(fullPath).getFileNameWithoutExtension();
                                                    if (!appName.equalsIgnoreCase("ApplicationFrameHost") &&
                                                        !appName.equalsIgnoreCase("SearchApp") &&
                                                        !appName.equalsIgnoreCase("audiodg"))
                                                    {
                                                        data.seenPids.insert(pid);
                                                        data.apps.push_back({ pid, appName, "Audio Stream" });
                                                    }
                                                }
                                                CloseHandle(hProc);
                                            }
                                        }
                                    }
                                    pCtrl2->Release();
                                }
                                pCtrl->Release();
                            }
                        }
                        pSessionEnum->Release();
                    }
                    pMgr->Release();
                }
                pDev->Release();
            }
            pEnum->Release();
        }

        if (SUCCEEDED(hrCo))
            CoUninitialize();

        // 2. Desktop Window Enumeration (finds all open applications and enriches window titles)
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));

        return data.apps;
    }
} // namespace dsd
