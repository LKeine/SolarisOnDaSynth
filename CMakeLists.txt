cmake_minimum_required(VERSION 3.22)

project(SolarisOnDaSynth VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SOLARIS_BUILD_PLUGIN      "Compiler le plugin (VST3 + Standalone)"          ON)
option(SOLARIS_BUILD_RENDER_TOOL "Compiler l'outil de rendu hors-ligne (WAV -> WAV)" OFF)

if(SOLARIS_BUILD_PLUGIN)
    # JUCE est telecharge automatiquement au premier "cmake -B build"
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(SolarisOnDaSynth
        COMPANY_NAME                "Solaris"
        PLUGIN_MANUFACTURER_CODE    Slrs
        PLUGIN_CODE                 Sods
        FORMATS                     VST3 Standalone
        PRODUCT_NAME                "SolarisOnDaSynth"
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Fx)

    target_sources(SolarisOnDaSynth PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(SolarisOnDaSynth PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(SolarisOnDaSynth
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(SOLARIS_BUILD_RENDER_TOOL)
    add_executable(solaris_render tools/solaris_render.cpp)
endif()
