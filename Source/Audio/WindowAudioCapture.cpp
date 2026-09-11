#include "Audio/WindowAudioCapture.h"
#include <psapi.h>
#include <propvarutil.h>
#include <iostream>
#include <unordered_set>

#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "ole32.lib")

namespace dsd
{
    // COM Async Activation Handler for Process Loopback
    class ActivateAudioInterfaceCompletionHandler : public IActivateAudioInterfaceCompletionHandler
    {
    public:
        ActivateAudioInterfaceCompletionHandler()
            : refCount(1), completedEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr))
        {
        }

        ~ActivateAudioInterfaceCompletionHandler()
        {
            if (completedEvent != nullptr)
                CloseHandle(completedEvent);
        }

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler))
            {
                *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
                AddRef();
                return S_OK;
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
            asyncOp = op;
            SetEvent(completedEvent);
            return S_OK;
        }

        HANDLE completedEvent{nullptr};
        IActivateAudioInterfaceAsyncOperation* asyncOp{nullptr};

    private:
        ULONG refCount;
    };

    // =========================================================================
    // WindowAudioCapture
    // =========================================================================
    WindowAudioCapture::WindowAudioCapture(DWORD targetPid, const juce::String& processName)
        : pid(targetPid), procName(processName)
    {
    }

    WindowAudioCapture::~WindowAudioCapture()
    {
        releaseResources();
    }

    void WindowAudioCapture::prepare(double sampleRate, int /*maxBlockSize*/)
    {
        targetSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;

        if (!isRunning.load())
        {
            shouldStop.store(false);
            ringBuffer.reset();
            captureThread = std::thread(&WindowAudioCapture::threadLoop, this);
        }
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
        if (!isRunning.load() || numSamples <= 0)
        {
            for (int ch = 0; ch < targetBuffer.getNumChannels(); ++ch)
                targetBuffer.clear(ch, 0, numSamples);
            return;
        }

        ringBuffer.read(targetBuffer.getArrayOfWritePointers(), targetBuffer.getNumChannels(), numSamples);
    }

    bool WindowAudioCapture::activateProcessLoopback(IAudioClient** outClient)
    {
        if (outClient == nullptr || pid == 0)
            return false;

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
            handler->Release();
            return false;
        }

        WaitForSingleObject(handler->completedEvent, 2500);

        HRESULT hrActivateResult = E_FAIL;
        IUnknown* unk = nullptr;

        if (handler->asyncOp != nullptr)
            handler->asyncOp->GetActivateResult(&hrActivateResult, &unk);

        bool success = false;
        if (SUCCEEDED(hrActivateResult) && unk != nullptr)
        {
            hr = unk->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void**>(outClient));
            if (SUCCEEDED(hr) && *outClient != nullptr)
                success = true;
            unk->Release();
        }

        if (asyncOp != nullptr)
            asyncOp->Release();

        handler->Release();
        return success;
    }

    void WindowAudioCapture::threadLoop()
    {
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        IAudioClient* client = nullptr;
        if (!activateProcessLoopback(&client))
        {
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        WAVEFORMATEX* pwfx = nullptr;
        HRESULT hr = client->GetMixFormat(&pwfx);
        if (FAILED(hr) || pwfx == nullptr)
        {
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        captureSampleRate = pwfx->nSamplesPerSec > 0 ? static_cast<double>(pwfx->nSamplesPerSec) : 48000.0;
        captureChannels = pwfx->nChannels > 0 ? pwfx->nChannels : 2;

        HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        hr = client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            0,
            0,
            pwfx,
            nullptr);

        if (FAILED(hr))
        {
            CoTaskMemFree(pwfx);
            CloseHandle(hEvent);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        client->SetEventHandle(hEvent);

        IAudioCaptureClient* captureClient = nullptr;
        hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&captureClient));
        if (FAILED(hr) || captureClient == nullptr)
        {
            CoTaskMemFree(pwfx);
            CloseHandle(hEvent);
            client->Release();
            if (SUCCEEDED(hrCo))
                CoUninitialize();
            return;
        }

        client->Start();
        isRunning.store(true);

        interpolator[0].reset();
        interpolator[1].reset();

        const bool isFloat = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                             (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                              reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
        const int bitsPerSample = pwfx->wBitsPerSample;

        juce::AudioBuffer<float> tempCaptureBuffer(2, 4096);

        while (!shouldStop.load())
        {
            DWORD waitRes = WaitForSingleObject(hEvent, 40);
            if (shouldStop.load())
                break;

            UINT32 packetLength = 0;
            hr = captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr))
                continue;

            while (packetLength > 0 && !shouldStop.load())
            {
                BYTE* pData = nullptr;
                UINT32 numFramesRead = 0;
                DWORD flags = 0;

                hr = captureClient->GetBuffer(&pData, &numFramesRead, &flags, nullptr, nullptr);
                if (FAILED(hr) || pData == nullptr)
                    break;

                if (numFramesRead > 0)
                {
                    if (tempCaptureBuffer.getNumSamples() < static_cast<int>(numFramesRead))
                        tempCaptureBuffer.setSize(2, numFramesRead, false, false, true);

                    float* leftDst = tempCaptureBuffer.getWritePointer(0);
                    float* rightDst = tempCaptureBuffer.getWritePointer(1);

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    {
                        tempCaptureBuffer.clear(0, numFramesRead);
                    }
                    else if (isFloat && bitsPerSample == 32)
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
                    else
                    {
                        tempCaptureBuffer.clear(0, numFramesRead);
                    }

                    // Resample to engine target rate (48 kHz) if needed
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
                hr = captureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr))
                    break;
            }
        }

        client->Stop();
        captureClient->Release();
        client->Release();
        CoTaskMemFree(pwfx);
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
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
    {
        auto* data = reinterpret_cast<WindowEnumData*>(lParam);

        if (!IsWindowVisible(hWnd))
            return TRUE;

        int titleLen = GetWindowTextLengthW(hWnd);
        if (titleLen <= 0)
            return TRUE;

        // Skip windows without minimize/maximize or child tooltips
        LONG style = GetWindowLongW(hWnd, GWL_STYLE);
        if (!(style & WS_VISIBLE) || (style & WS_CHILD))
            return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid == 0 || pid == GetCurrentProcessId())
            return TRUE;

        if (data->seenPids.count(pid) > 0)
            return TRUE;

        std::vector<wchar_t> titleBuf(titleLen + 1);
        GetWindowTextW(hWnd, titleBuf.data(), titleLen + 1);
        juce::String title(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(titleBuf.data())));

        // Filter system windows
        if (title == "Program Manager" || title == "Settings" || title == "Windows Input Experience")
            return TRUE;

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

        data->seenPids.insert(pid);
        data->apps.push_back({ pid, appName, title });
        return TRUE;
    }

    std::vector<RunningAppInfo> WindowAudioCapture::getRunningApplications()
    {
        WindowEnumData data;
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.apps;
    }
} // namespace dsd
