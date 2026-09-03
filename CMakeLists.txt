cmake_minimum_required(VERSION 3.22)

project(PHOQER VERSION 0.1.0 LANGUAGES CXX)

set(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/iPlug2" CACHE PATH "iPlug2 root directory")
if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
    message(FATAL_ERROR
        "iPlug2 is missing. Run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${IPLUG2_DIR}/Dependencies/IPlug/VST3_SDK/CMakeLists.txt"
   OR NOT EXISTS "${IPLUG2_DIR}/Dependencies/IPlug/CLAP_SDK/include/clap/clap.h")
    message(FATAL_ERROR
        "The iPlug2 format SDKs are missing. Run: powershell -ExecutionPolicy Bypass -File Tools/BootstrapIPlug2.ps1")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

if(WIN32)
    set_source_files_properties(resources/PHOQERResources.rc PROPERTIES
        COMPILE_FLAGS "/I\"${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts\"")
endif()

set(PHOQER_CORE_SOURCES
    Source/Core/AudioBuffer.h
    Source/Core/DspPrimitives.h
    Source/Core/FaceTelemetry.h
    Source/Core/MidiEvent.h
    Source/Core/PhoqerEngine.cpp
    Source/Core/PhoqerEngine.h
    Source/Core/PhoqerParameters.h
    Source/Core/PhoqerTypes.h
    Source/Core/Random.h
    Source/Core/SealCharacter.h
    Source/DSP/BehaviourEngine.h
    Source/DSP/CheapSpace.cpp
    Source/DSP/CheapSpace.h
    Source/DSP/Exciter.h
    Source/DSP/FormantBank.cpp
    Source/DSP/FormantBank.h
    Source/DSP/OutputStage.h
    Source/DSP/PitchGesture.h
    Source/DSP/SmoothedRandom.h
    Source/DSP/ThroatStage.h
    Source/Voice/SealVoice.cpp
    Source/Voice/SealVoice.h
    Source/Voice/VoicePersonality.h)

iplug_add_plugin(PHOQER
    SOURCES
        Source/Phoqer.cpp
        Source/Phoqer.h
        config.h
        resources/resource.h
        resources/PHOQERResources.rc
        ${PHOQER_CORE_SOURCES}
    RESOURCES
        resources/fonts/Roboto-Regular.ttf
    FORMATS
        APP
        VST3
        CLAP
    DEFINES
        SAMPLE_TYPE_FLOAT)

option(PHOQER_BUILD_SANITY "Build the offline core sanity executable" ON)
if(PHOQER_BUILD_SANITY)
    add_executable(PHOQERCoreSanity
        Source/Diagnostics/CoreSanity.h
        Source/Diagnostics/CoreSanityMain.cpp
        ${PHOQER_CORE_SOURCES})
    target_include_directories(PHOQERCoreSanity PRIVATE Source)
    target_compile_features(PHOQERCoreSanity PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(PHOQERCoreSanity PRIVATE /W4 /permissive-)
    else()
        target_compile_options(PHOQERCoreSanity PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
