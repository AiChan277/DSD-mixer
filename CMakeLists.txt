cmake_minimum_required(VERSION 3.22)

project(DSDMixer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/utf-8)
endif()

# Add local JUCE repository
add_subdirectory(JUCE)

# Add IXWebSocket for DSDCP WebSocket transport (Stage C)
include(FetchContent)
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.6
)
FetchContent_MakeAvailable(ixwebsocket)

# Embed binary assets (fader PNG) into the executable
juce_add_binary_data(DSDMixerBinaryData
    SOURCES
        "LOGO.png"
        "Fader.png"
        "OnButton.png"
        "OffButton.png"
        "Knob.png"
        "BtnVst.png"
        "BtnDm.png"
)

juce_add_gui_app(DSDMixer
    PRODUCT_NAME "DSD Mixer"
    VERSION "1.0.0"
    COMPANY_NAME "DSD Audio"
    BUNDLE_ID "com.dsd.mixer"
)

target_sources(DSDMixer PRIVATE
    Source/Resources/DSDMixer.rc
    Source/Main.cpp
    Source/Application/DSDApplication.h
    Source/Application/DSDApplication.cpp
    Source/Application/CrashReporter.h
    Source/Application/CrashReporter.cpp
    Source/Audio/AudioTypes.h
    Source/Audio/ProcessCpuTracker.h
    Source/Audio/ProcessCpuTracker.cpp
    Source/Audio/AudioInputSource.h
    Source/Audio/WindowAudioCapture.h
    Source/Audio/WindowAudioCapture.cpp
    Source/Audio/AudioDeviceManager.h
    Source/Audio/AudioDeviceManager.cpp
    Source/Audio/MultiDeviceManager.h
    Source/Audio/MultiDeviceManager.cpp
    Source/Audio/AudioEngine.h
    Source/Audio/AudioEngine.cpp
    Source/Scheduler/DSPScheduler.h
    Source/Scheduler/DSPScheduler.cpp
    Source/DSP/GainProcessor.h
    Source/DSP/PanProcessor.h
    Source/DSP/MeterProcessor.h
    Source/DSP/MeterProcessor.cpp
    Source/DSP/BiquadFilter.h
    Source/Channel/AudioChannel.h
    Source/Channel/AudioChannel.cpp
    Source/Channel/ChannelManager.h
    Source/Channel/ChannelManager.cpp
    Source/Routing/RoutingEngine.h
    Source/Routing/RoutingEngine.cpp
    Source/Routing/AutoDucker.cpp
    Source/Bus/AudioBus.h
    Source/Bus/AudioBus.cpp
    Source/Bus/BusManager.h
    Source/Bus/BusManager.cpp
    Source/Output/OutputBus.h
    Source/Output/OutputBus.cpp
    Source/Output/OutputManager.h
    Source/Output/OutputManager.cpp
    Source/Plugin/PluginRack.h
    Source/Plugin/PluginRack.cpp
    Source/Plugin/PluginManager.h
    Source/Plugin/PluginManager.cpp
    Source/Plugin/BuiltInPlugins.cpp
    Source/Session/SessionManager.h
    Source/Session/SessionManager.cpp
    Source/Session/SceneManager.cpp
    Source/Session/AppSettings.h
    Source/Session/AppSettings.cpp
    Source/UI/DSDLookAndFeel.h
    Source/UI/DSDLookAndFeel.cpp
    Source/UI/MeterComponent.h
    Source/UI/MeterComponent.cpp
    Source/UI/FaderComponent.h
    Source/UI/FaderComponent.cpp
    Source/UI/DSDKnob.h
    Source/UI/DSDKnob.cpp
    Source/UI/DSDConsoleButton.h
    Source/UI/DSDConsoleButton.cpp
    Source/UI/ChannelStrip.h
    Source/UI/ChannelStrip.cpp
    Source/UI/OutputBayStrip.h
    Source/UI/OutputBayStrip.cpp
    Source/UI/OutputBayPanel.h
    Source/UI/OutputBayPanel.cpp
    Source/UI/RoutingMatrixDialog.h
    Source/UI/RoutingMatrixDialog.cpp
    Source/UI/PerformanceMonitorDialog.h
    Source/UI/PerformanceMonitorDialog.cpp
    Source/UI/ToolsDialogs.h
    Source/UI/ToolsDialogs.cpp
    Source/UI/PluginRackDialog.h
    Source/UI/PluginRackDialog.cpp
    Source/UI/StageInspectorDialog.h
    Source/UI/StageInspectorDialog.cpp
    Source/UI/ControlServerDialog.h
    Source/UI/ControlServerDialog.cpp
    Source/UI/TopBar.h
    Source/UI/TopBar.cpp
    Source/UI/MainView.h
    Source/UI/MainView.cpp
    Source/UI/MainWindow.h
    Source/UI/MainWindow.cpp
    Source/UI/DSDSystemTray.h
    Source/UI/DSDSystemTray.cpp
    # Control layer (Stage A, B & C)
    Source/Control/DSDCPTypes.h
    Source/Control/DSDCPSerializer.h
    Source/Control/DSDCPSerializer.cpp
    Source/Control/CommandDispatcher.h
    Source/Control/CommandDispatcher.cpp
    Source/Control/MixerStateSnapshot.h
    Source/Control/MixerStateSnapshot.cpp
    Source/Control/StateObserver.h
    Source/Control/StateObserver.cpp
    Source/Control/ClientManager.h
    Source/Control/ClientManager.cpp
    Source/Control/WebSocketServer.h
    Source/Control/WebSocketServer.cpp
    Source/Control/ControlServer.h
    Source/Control/ControlServer.cpp
)

target_include_directories(DSDMixer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(DSDMixer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
)

if (MSVC)
    # Map file: resolves crash-report addresses to functions without shipping symbols.
    target_link_options(DSDMixer PRIVATE "/MAP")
endif()

target_link_libraries(DSDMixer PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    DSDMixerBinaryData
    ixwebsocket
    ws2_32
    crypt32
    avrt
    winmm
)

# Automated test target for DSP, 16x4 routing, and multicore engine
add_executable(AudioEngineTests
    Tests/AudioEngineTests.cpp
    Source/DSP/MeterProcessor.cpp
    Source/Channel/AudioChannel.cpp
    Source/Channel/ChannelManager.cpp
    Source/Scheduler/DSPScheduler.cpp
    Source/Routing/RoutingEngine.cpp
    Source/Routing/AutoDucker.cpp
    Source/Bus/AudioBus.cpp
    Source/Bus/BusManager.cpp
    Source/Output/OutputBus.cpp
    Source/Output/OutputManager.cpp
    Source/Plugin/PluginRack.cpp
    Source/Plugin/PluginManager.cpp
    Source/Plugin/BuiltInPlugins.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/ProcessCpuTracker.cpp
    Source/Session/SessionManager.cpp
    Source/Session/SceneManager.cpp
    Source/Audio/AudioDeviceManager.cpp
    Source/Audio/MultiDeviceManager.cpp
    Source/Audio/WindowAudioCapture.cpp
)

target_include_directories(AudioEngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(AudioEngineTests PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
)

target_link_libraries(AudioEngineTests PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_processors
    avrt
    winmm
)

# -------------------------------------------------------------------------
# Stage A: DSDCP protocol and CommandDispatcher tests
# -------------------------------------------------------------------------
add_executable(ControlProtocolTests
    Tests/ControlProtocolTests.cpp
    # Control layer (Stage A, B & C)
    Source/Control/DSDCPSerializer.cpp
    Source/Control/CommandDispatcher.cpp
    Source/Control/MixerStateSnapshot.cpp
    Source/Control/StateObserver.cpp
    Source/Control/ClientManager.cpp
    Source/Control/WebSocketServer.cpp
    Source/Control/ControlServer.cpp
    # Engine subsystems needed by CommandDispatcher
    Source/Audio/AudioEngine.cpp
    Source/Audio/ProcessCpuTracker.cpp
    Source/Audio/AudioDeviceManager.cpp
    Source/Audio/MultiDeviceManager.cpp
    Source/Audio/WindowAudioCapture.cpp
    Source/Channel/AudioChannel.cpp
    Source/Channel/ChannelManager.cpp
    Source/Scheduler/DSPScheduler.cpp
    Source/Routing/RoutingEngine.cpp
    Source/Routing/AutoDucker.cpp
    Source/Bus/AudioBus.cpp
    Source/Bus/BusManager.cpp
    Source/Output/OutputBus.cpp
    Source/Output/OutputManager.cpp
    Source/Plugin/PluginRack.cpp
    Source/Plugin/PluginManager.cpp
    Source/Plugin/BuiltInPlugins.cpp
    Source/Session/SessionManager.cpp
    Source/Session/SceneManager.cpp
    Source/DSP/MeterProcessor.cpp
)

target_include_directories(ControlProtocolTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(ControlProtocolTests PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
)

target_link_libraries(ControlProtocolTests PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_processors
    ixwebsocket
    ws2_32
    crypt32
    avrt
    winmm
)
