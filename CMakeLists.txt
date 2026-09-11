cmake_minimum_required(VERSION 3.22)

project(DSDMixer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add local JUCE repository
add_subdirectory(JUCE)

juce_add_gui_app(DSDMixer
    PRODUCT_NAME "DSD Mixer"
    VERSION "0.1.0"
    COMPANY_NAME "DSD Audio"
    BUNDLE_ID "com.dsd.mixer"
)

target_sources(DSDMixer PRIVATE
    Source/Main.cpp
    Source/Application/DSDApplication.h
    Source/Application/DSDApplication.cpp
    Source/Audio/AudioTypes.h
    Source/Audio/AudioInputSource.h
    Source/Audio/AudioDeviceManager.h
    Source/Audio/AudioDeviceManager.cpp
    Source/Audio/AudioEngine.h
    Source/Audio/AudioEngine.cpp
    Source/DSP/GainProcessor.h
    Source/DSP/PanProcessor.h
    Source/DSP/MeterProcessor.h
    Source/DSP/MeterProcessor.cpp
    Source/Channel/AudioChannel.h
    Source/Channel/AudioChannel.cpp
    Source/Channel/ChannelManager.h
    Source/Channel/ChannelManager.cpp
    Source/Routing/RoutingEngine.h
    Source/Routing/RoutingEngine.cpp
    Source/Master/MasterBus.h
    Source/Master/MasterBus.cpp
    Source/UI/DSDLookAndFeel.h
    Source/UI/DSDLookAndFeel.cpp
    Source/UI/MeterComponent.h
    Source/UI/MeterComponent.cpp
    Source/UI/FaderComponent.h
    Source/UI/FaderComponent.cpp
    Source/UI/ChannelStrip.h
    Source/UI/ChannelStrip.cpp
    Source/UI/MasterStrip.h
    Source/UI/MasterStrip.cpp
    Source/UI/TopBar.h
    Source/UI/TopBar.cpp
    Source/UI/MainView.h
    Source/UI/MainView.cpp
    Source/UI/MainWindow.h
    Source/UI/MainWindow.cpp
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
)

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
)

# Test target for automated DSP & engine verification
add_executable(AudioEngineTests
    Tests/AudioEngineTests.cpp
    Source/DSP/MeterProcessor.cpp
    Source/Channel/AudioChannel.cpp
    Source/Channel/ChannelManager.cpp
    Source/Routing/RoutingEngine.cpp
    Source/Master/MasterBus.cpp
)

target_include_directories(AudioEngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(AudioEngineTests PRIVATE
    juce::juce_core
    juce::juce_audio_basics
)
