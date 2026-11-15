cmake_minimum_required(VERSION 3.22)

set(PHOQER_VERSION "1.1.0" CACHE STRING "PHOQER version (a test build of a fake update sets 1.1.1)")
project(PHOQER VERSION ${PHOQER_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(PHOQER_JUCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE" CACHE PATH "Existing JUCE source")
add_subdirectory("${PHOQER_JUCE_SOURCE}" vendor/JUCE)

juce_add_binary_data(PHOQER_UIAssets
    HEADER_NAME PhoqerUIAssets.h NAMESPACE PhoqerUIAssets
    SOURCES
        resources/ui/Fonts/Silkscreen-Regular.ttf
        resources/ui/Fonts/Silkscreen-Bold.ttf
        resources/ui/Fonts/PressStart2P-Regular.ttf
        resources/ui/Shared/Roboto-Regular.ttf
        resources/ui/Purple/purple_seal.png
        resources/ui/Ice/ice_seal.png
        resources/ui/Kyrax/kyrax_seal.png
        resources/ui/Red/red_seal.png
        resources/branding/app/phoqer-app-icon-256.png
        resources/branding/app/phoqer-app-icon-32.png
        resources/branding/app/kyrax-badge-32.png
        LICENSE
        NOTICE.md
        THIRD_PARTY_NOTICES.md
        LICENSES/Apache-2.0.txt
        LICENSES/Roboto-NOTICE.txt
        resources/ui/Fonts/OFL-Silkscreen.txt
        resources/ui/Fonts/OFL-PressStart2P.txt)

# KYRAX's voice data ships as a separate file beside the app (Voices/) and inside the VST3 bundle
# (Resources/Voices/), never compiled into PHOQER: AGPL code and the paid data stay separate works.
# It lives outside the public source tree; PHOQER_KYRAX_FILE picks which file (a test build can carry a changed voice).
set(PHOQER_KYRAX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Voices/KYRAX.phqvoice" CACHE FILEPATH "KYRAX voice data shipped beside the app")
# KYRAX's voice now comes from the activation server with the key, so builds leave it out. A test build can still
# bundle it (-DPHOQER_BUNDLE_KYRAX_VOICE=ON); without the file it is left out either way.
option(PHOQER_BUNDLE_KYRAX_VOICE "Copy KYRAX.phqvoice beside the app and into the VST3 (test builds only)" OFF)
if(PHOQER_BUNDLE_KYRAX_VOICE AND EXISTS "${PHOQER_KYRAX_FILE}")
    configure_file("${PHOQER_KYRAX_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/Voices/KYRAX.phqvoice" COPYONLY)
    set(PHOQER_BUNDLE_KYRAX ON)
else()
    message(STATUS "KYRAX voice not bundled: it comes from the activation server with the key")
    set(PHOQER_BUNDLE_KYRAX OFF)
endif()

# App icon. Windows takes a hand-built multi-size .ico (added below) so every size is drawn for its
# size; macOS takes a 1024 px drawing on Apple's icon grid (margin + shadow), so it matches other apps
# and stays sharp; elsewhere JUCE builds the icon from the 32 px and 256 px PNGs.
set(PHOQER_ICONS)
if(APPLE)
    set(PHOQER_ICONS ICON_BIG resources/branding/app/phoqer-app-icon-mac-1024.png)
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PHOQER_ICONS
        ICON_BIG resources/branding/app/phoqer-app-icon-256.png
        ICON_SMALL resources/branding/app/phoqer-app-icon-32.png)
endif()

# AU on macOS as well; Windows builds VST3 and the standalone app.
set(PHOQER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PHOQER_FORMATS AU)
endif()

juce_add_plugin(PHOQER
    COMPANY_NAME "LWNX DSP"
    COMPANY_WEBSITE "https://lewonn.com/phoqer"
    BUNDLE_ID "io.github.lew0nn.phoqer"
    PLUGIN_MANUFACTURER_CODE Lwnx
    PLUGIN_CODE Phqr
    FORMATS ${PHOQER_FORMATS}
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

set(PHOQER_KEY_SOURCES
    Source/LicenceKey.h
    Source/ThirdParty/monocypher/monocypher.c
    Source/ThirdParty/monocypher/monocypher-ed25519.c)

set(PHOQER_ENGINE_SOURCES
    Source/Core/AudioBuffer.h
    Source/Core/FishSprite.h
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
    Source/DSP/FdnSpace.cpp
    Source/DSP/FdnSpace.h
    Source/DSP/Chorus.h
    Source/DSP/FishSong.h
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
    Source/Voice/Gestures.h
    Source/Voice/LevelTables.h
    Source/Voice/VoicePersonality.h)

target_sources(PHOQER PRIVATE
    Source/Licence.cpp
    Source/Licence.h
    Source/Updates.cpp
    Source/Updates.h
    ${PHOQER_KEY_SOURCES}
    Source/VoiceSelection.cpp
    Source/VoiceSelection.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    ${PHOQER_ENGINE_SOURCES}
    Source/UI/Editor.cpp
    Source/UI/Editor.h
    Source/UI/EditorShared.h
    Source/UI/EditorMenus.cpp
    Source/UI/EditorDialogs.cpp
    Source/UI/EditorVoices.cpp
    Source/UI/EditorUpdates.cpp
    Source/UI/History.cpp
    Source/UI/History.h
    Source/UI/Knob.cpp
    Source/UI/Knob.h
    Source/UI/Nitro98.cpp
    Source/UI/Nitro98.h
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
    Source/UI/Views.h
    Source/UI/Controls98.cpp
    Source/UI/FishAndBlueScreen98.cpp
    Source/UI/PianoView.cpp
    Source/UI/Panels98.cpp
    Source/UI/Dialog98.cpp)

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
    juce::juce_cryptography
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
    "${PHOQER_JUCE_SOURCE}/LICENSE.md")

add_custom_command(TARGET PHOQER_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/PHOQER-Licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PHOQER_NOTICE_FILES}
        "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/PHOQER-Licenses"
    COMMENT "Embedding PHOQER and JUCE license notices in the VST3 bundle")

# Beside the .exe on Windows; inside the .app's Resources on macOS (code signing allows only code in MacOS/).
if(APPLE)
    set(PHOQER_STANDALONE_LICENSES "$<TARGET_FILE_DIR:PHOQER_Standalone>/../Resources/PHOQER-Licenses")
else()
    set(PHOQER_STANDALONE_LICENSES "$<TARGET_FILE_DIR:PHOQER_Standalone>/PHOQER-Licenses")
endif()
add_custom_command(TARGET PHOQER_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PHOQER_STANDALONE_LICENSES}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PHOQER_NOTICE_FILES} "${PHOQER_STANDALONE_LICENSES}"
    COMMENT "Collecting PHOQER and JUCE license notices for Standalone")

if(TARGET PHOQER_AU)
    add_custom_command(TARGET PHOQER_AU POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PHOQER_AU>/../Resources/PHOQER-Licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PHOQER_NOTICE_FILES} "$<TARGET_FILE_DIR:PHOQER_AU>/../Resources/PHOQER-Licenses"
        COMMENT "Embedding PHOQER and JUCE license notices in the AU bundle")
endif()

option(PHOQER_BUILD_SANITY "Build the offline core sanity executable" ON)
option(PHOQER_BUILD_VISUAL_ACCEPTANCE "Build the window-free UI acceptance renderer" OFF)
option(PHOQER_BUILD_HOST_CHECKS "Build the automated VST3 host checks" OFF)
if(PHOQER_BUILD_HOST_CHECKS)
    juce_add_console_app(PHOQERVst3HostChecks PRODUCT_NAME "PHOQER VST3 Host Checks")
    target_sources(PHOQERVst3HostChecks PRIVATE Tests/Vst3HostChecks.cpp)
    target_include_directories(PHOQERVst3HostChecks PRIVATE Source)
    target_compile_definitions(PHOQERVst3HostChecks PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_LV2=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PHOQERVst3HostChecks PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
if(PHOQER_BUILD_VISUAL_ACCEPTANCE)
    juce_add_console_app(PHOQERPluginChecks PRODUCT_NAME "PHOQER Plugin Checks")
    target_sources(PHOQERPluginChecks PRIVATE Tests/PluginChecks.cpp)
    target_include_directories(PHOQERPluginChecks PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/PHOQER_artefacts/JuceLibraryCode")
    target_link_libraries(PHOQERPluginChecks PRIVATE PHOQER PHOQER_UIAssets juce::juce_audio_utils juce::juce_dsp)
    if(MSVC)
        target_link_options(PHOQERPluginChecks PRIVATE /DEBUG)
    endif()
    juce_add_console_app(PHOQERUIAcceptance PRODUCT_NAME "PHOQER UI Acceptance")
    target_sources(PHOQERUIAcceptance PRIVATE Tests/UiAcceptance.cpp)
    target_include_directories(PHOQERUIAcceptance PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/PHOQER_artefacts/JuceLibraryCode")
    target_link_libraries(PHOQERUIAcceptance PRIVATE PHOQER PHOQER_UIAssets juce::juce_audio_utils juce::juce_dsp)
endif()
if(PHOQER_BUILD_SANITY)
    add_executable(PHOQERPerformanceSanity Tests/PerformanceSanity.cpp ${PHOQER_ENGINE_SOURCES})
    target_include_directories(PHOQERPerformanceSanity PRIVATE Source)
    # Fails when the sound changes: compares every voice and mood with Tests/SoundPrints.txt (test-only code lives in Tests/, never in Source/).
    add_executable(PHOQERSoundPrint Tests/SoundPrint.cpp ${PHOQER_ENGINE_SOURCES})
    target_include_directories(PHOQERSoundPrint PRIVATE Source)
    # Fails when a mood is not as loud as CALL: measures Source/Voice/LevelTables.h against the engine.
    add_executable(PHOQERLevels Tests/Levels.cpp ${PHOQER_ENGINE_SOURCES})
    target_include_directories(PHOQERLevels PRIVATE Source)
    add_executable(PHOQERCoreSanity
        Tests/CoreSanity.h
        Tests/CoreSanityMain.cpp
        Source/Core/PhoqerEngine.cpp
        Source/DSP/FdnSpace.cpp
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




juce_add_console_app(PHOQERTestChecks PRODUCT_NAME "PHOQER Test Checks")
target_sources(PHOQERTestChecks PRIVATE Tests/TestBuildChecks.cpp)
target_include_directories(PHOQERTestChecks PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/PHOQER_artefacts/JuceLibraryCode")
target_link_libraries(PHOQERTestChecks PRIVATE PHOQER PHOQER_UIAssets juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)
juce_add_console_app(PHOQERShowcase PRODUCT_NAME "PHOQER Showcase")
target_sources(PHOQERShowcase PRIVATE Tests/Showcase.cpp)
target_include_directories(PHOQERShowcase PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/PHOQER_artefacts/JuceLibraryCode")
target_link_libraries(PHOQERShowcase PRIVATE PHOQER PHOQER_UIAssets juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography)

if(PHOQER_BUNDLE_KYRAX)
add_custom_command(TARGET PHOQER_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PHOQER_Standalone>/Voices"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/Voices/KYRAX.phqvoice" "$<TARGET_FILE_DIR:PHOQER_Standalone>/Voices/KYRAX.phqvoice")
add_custom_command(TARGET PHOQER_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/Voices"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/Voices/KYRAX.phqvoice" "$<TARGET_FILE_DIR:PHOQER_VST3>/../Resources/Voices/KYRAX.phqvoice")
endif()
