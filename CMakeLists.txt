cmake_minimum_required(VERSION 3.22)

project(PHOQER VERSION 0.1.0 LANGUAGES CXX)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(vendor/JUCE)

juce_add_plugin(PHOQER
    COMPANY_NAME "LWNX DSP"
    BUNDLE_ID "com.lwnxdsp.phoqer"
    PLUGIN_MANUFACTURER_CODE Lwnx
    PLUGIN_CODE Phqr
    FORMATS VST3 Standalone
    PRODUCT_NAME "PHOQER"
    VERSION "0.1.0"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(PHOQER)

target_sources(PHOQER PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Core/FaceTelemetry.h
    Source/Core/PhoqerEngine.cpp
    Source/Core/PhoqerEngine.h
    Source/Core/PhoqerParameters.h
    Source/Core/PhoqerTypes.h
    Source/Core/Random.h
    Source/DSP/BehaviourEngine.h
    Source/DSP/CheapDigitalStage.cpp
    Source/DSP/CheapDigitalStage.h
    Source/DSP/CheapSpace.cpp
    Source/DSP/CheapSpace.h
    Source/DSP/Exciter.h
    Source/DSP/FormantBank.cpp
    Source/DSP/FormantBank.h
    Source/DSP/OutputStage.h
    Source/DSP/PitchGesture.h
    Source/DSP/SmoothedRandom.h
    Source/DSP/ThroatStage.h
    Source/Voice/SealSound.h
    Source/Voice/SealVoice.cpp
    Source/Voice/SealVoice.h
    Source/Voice/VoicePersonality.h
    Source/Diagnostics/CoreSanity.h)

target_compile_features(PHOQER PRIVATE cxx_std_17)
target_include_directories(PHOQER PRIVATE Source)

target_compile_definitions(PHOQER PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PHOQER PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

option(PHOQER_BUILD_SANITY "Build the offline core sanity executable" ON)
if(PHOQER_BUILD_SANITY)
    juce_add_console_app(PHOQERCoreSanity PRODUCT_NAME "PHOQER Core Sanity")
    juce_generate_juce_header(PHOQERCoreSanity)
    target_sources(PHOQERCoreSanity PRIVATE
        Source/Diagnostics/CoreSanityMain.cpp
        Source/PluginProcessor.cpp
        Source/Core/PhoqerEngine.cpp
        Source/DSP/CheapDigitalStage.cpp
        Source/DSP/CheapSpace.cpp
        Source/DSP/FormantBank.cpp
        Source/Voice/SealVoice.cpp)
    target_include_directories(PHOQERCoreSanity PRIVATE Source)
    target_compile_features(PHOQERCoreSanity PRIVATE cxx_std_17)
    target_compile_definitions(PHOQERCoreSanity PRIVATE
        JucePlugin_Name="PHOQER"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(PHOQERCoreSanity PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
