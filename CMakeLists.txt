cmake_minimum_required(VERSION 3.22)

project(PHOQER VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(vendor/JUCE)

juce_add_binary_data(PHOQER_UIAssets
    HEADER_NAME PhoqerUIAssets.h NAMESPACE PhoqerUIAssets
    SOURCES
        resources/ui/Fonts/Silkscreen-Regular.ttf
        resources/ui/Fonts/Silkscreen-Bold.ttf
        resources/ui/Fonts/PressStart2P-Regular.ttf
        resources/ui/Shared/Roboto-Regular.ttf
        resources/ui/Purple/purple_seal.png
        resources/ui/Ice/ice_seal.png
        resources/ui/Red/red_seal.png
        resources/branding/app/phoqer-app-icon-256.png
        resources/branding/app/phoqer-app-icon-32.png
        LICENSE
        NOTICE.md
        THIRD_PARTY_NOTICES.md
        LICENSES/Apache-2.0.txt
        LICENSES/Roboto-NOTICE.txt
        resources/ui/Fonts/OFL-Silkscreen.txt
        resources/ui/Fonts/OFL-PressStart2P.txt)

# App icon. Windows takes a hand-built multi-size .ico (added below) so every size is drawn for its
# size; elsewhere JUCE builds the icon from the 32 px and 256 px PNGs.
set(PHOQER_ICONS)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PHOQER_ICONS
        ICON_BIG resources/branding/app/phoqer-app-icon-256.png
        ICON_SMALL resources/branding/app/phoqer-app-icon-32.png)
endif()

juce_add_plugin(PHOQER
    COMPANY_NAME "LWNX DSP"
    BUNDLE_ID "io.github.lew0nn.phoqer"
    PLUGIN_MANUFACTURER_CODE Lwnx
    PLUGIN_CODE Phqr
    FORMATS VST3 Standalone
    PRODUCT_NAME "PHOQER"
    DESCRIPTION "PHOQER synthetic seal vocal instrument"
    ${PHOQER_ICONS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(PHOQER_ENGINE_SOURCES
    Source/Core/AudioBuffer.h
    Source/Core/CharacterPreset.h
    Source/Core/DspPrimitives.h
    Source/Core/FaceTelemetry.h
    Source/Core/MidiEvent.h
    Source/Core/PhoqerEngine.cpp
    Source/Core/PhoqerEngine.h
    Source/Core/PhoqerTypes.h
    Source/Core/Random.h
    Source/Core/SealCharacter.h
    Source/DSP/BehaviourEngine.h
    Source/DSP/CheapSpace.cpp
    Source/DSP/CheapSpace.h
    Source/DSP/Chorus.h
    Source/DSP/FofBank.cpp
    Source/DSP/FofBank.h
    Source/DSP/FofFormant.h
    Source/DSP/FofGrain.h
    Source/DSP/GlottalPulse.h
    Source/DSP/Limiter.h
    Source/DSP/OutputStage.h
    Source/DSP/SmoothedRandom.h
    Source/Voice/SealVoice.cpp
    Source/Voice/SealVoice.h
    Source/Voice/VoicePersonality.h)

target_sources(PHOQER PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    ${PHOQER_ENGINE_SOURCES}
    Source/UI/Editor.cpp
    Source/UI/Editor.h
    Source/UI/History.cpp
    Source/UI/History.h
    Source/UI/Knob.cpp
    Source/UI/Knob.h
    Source/UI/Logo.cpp
    Source/UI/Logo.h
    Source/UI/Portrait.cpp
    Source/UI/Portrait.h
    Source/UI/Presets.cpp
    Source/UI/Presets.h
    Source/UI/Recorder.cpp
    Source/UI/Recorder.h
    Source/UI/Settings.cpp
    Source/UI/Settings.h
    Source/UI/StandaloneApp.cpp
    Source/UI/Style.cpp
    Source/UI/Style.h
    Source/UI/Views.cpp
    Source/UI/Views.h)

target_include_directories(PHOQER PRIVATE Source)

target_compile_definitions(PHOQER PUBLIC
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0)

target_link_libraries(PHOQER PRIVATE
    PHOQER_UIAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

juce_generate_juce_header(PHOQER)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND TARGET PHOQER_Standalone)
    target_sources(PHOQER_Standalone PRIVATE resources/branding/app/phoqer-app-icon.rc)
endif()

set(PHOQER_NOTICE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Apache-2.0.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Roboto-NOTICE.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/ui/Fonts/OFL-Silkscreen.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/ui/Fonts/OFL-PressStart2P.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE/LICENSE.md")

add_custom_command(TARGET PHOQER_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/PHOQER-Licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PHOQER_NOTICE_FILES}
        "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/PHOQER-Licenses"
    COMMENT "Embedding PHOQER and JUCE license notices in the VST3 bundle")

add_custom_command(TARGET PHOQER_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:PHOQER_Standalone>/PHOQER-Licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PHOQER_NOTICE_FILES}
        "$<TARGET_FILE_DIR:PHOQER_Standalone>/PHOQER-Licenses"
    COMMENT "Collecting PHOQER and JUCE license notices for Standalone")

option(PHOQER_BUILD_SANITY "Build the offline core sanity executable" ON)
option(PHOQER_BUILD_VISUAL_ACCEPTANCE "Build the window-free UI acceptance renderer" OFF)
if(PHOQER_BUILD_VISUAL_ACCEPTANCE)
    juce_add_console_app(PHOQERUIAcceptance PRODUCT_NAME "PHOQER UI Acceptance")
    target_sources(PHOQERUIAcceptance PRIVATE Source/Diagnostics/UiAcceptance.cpp)
    target_include_directories(PHOQERUIAcceptance PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/PHOQER_artefacts/JuceLibraryCode")
    target_link_libraries(PHOQERUIAcceptance PRIVATE PHOQER PHOQER_UIAssets juce::juce_audio_utils juce::juce_dsp)
endif()
if(PHOQER_BUILD_SANITY)
    add_executable(PHOQERCoreSanity
        Source/Diagnostics/CoreSanity.h
        Source/Diagnostics/CoreSanityMain.cpp
        Source/Core/PhoqerEngine.cpp
        Source/DSP/CheapSpace.cpp
        Source/DSP/FofBank.cpp
        Source/Voice/SealVoice.cpp)
    target_include_directories(PHOQERCoreSanity PRIVATE Source)
    target_compile_features(PHOQERCoreSanity PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(PHOQERCoreSanity PRIVATE /W4 /permissive-)
    else()
        target_compile_options(PHOQERCoreSanity PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
