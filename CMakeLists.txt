cmake_minimum_required(VERSION 3.22)

project(MP3Glitch VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MP3GLITCH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(MP3GLITCH_BUILD_TESTS "Build plug-in integration tests" ON)
set(MP3GLITCH_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")

if(MP3GLITCH_BUILD_PLUGIN)
    include(FetchContent)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(MP3GLITCH_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND MP3GLITCH_FORMATS AU)
    endif()

    juce_add_plugin(MP3Glitch
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "EsionHsrahLatigid"
        BUNDLE_ID "jp.ehl.mp3glitch"
        PRODUCT_NAME "MP3 Glitch"
        DESCRIPTION "Real-time MP3 data-corruption simulation effect"
        PLUGIN_MANUFACTURER_CODE EHL_
        PLUGIN_CODE Mp3G
        FORMATS ${MP3GLITCH_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE kAudioUnitType_Effect
        VST3_CATEGORIES Fx Distortion
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(MP3Glitch)

    target_sources(MP3Glitch PRIVATE
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

    target_compile_definitions(MP3Glitch PUBLIC
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(MP3Glitch
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    set(mp3glitch_juce_products "${CMAKE_CURRENT_BINARY_DIR}/MP3Glitch_artefacts/$<CONFIG>")
    set(mp3glitch_staged_products "${MP3GLITCH_ARTIFACT_DIR}/$<CONFIG>")
    set(mp3glitch_artifact_dependencies MP3Glitch_VST3 MP3Glitch_Standalone)
    set(mp3glitch_artifact_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${mp3glitch_staged_products}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${mp3glitch_staged_products}/VST3"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${mp3glitch_staged_products}/Standalone"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${mp3glitch_juce_products}/VST3" "${mp3glitch_staged_products}/VST3"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${mp3glitch_juce_products}/Standalone" "${mp3glitch_staged_products}/Standalone")

    if(TARGET MP3Glitch_AU)
        list(APPEND mp3glitch_artifact_dependencies MP3Glitch_AU)
        list(APPEND mp3glitch_artifact_commands
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${mp3glitch_staged_products}/AU"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${mp3glitch_juce_products}/AU" "${mp3glitch_staged_products}/AU")
    endif()

    if(APPLE)
        list(APPEND mp3glitch_artifact_commands
            COMMAND codesign --force --deep --sign -
                    "${mp3glitch_staged_products}/VST3/MP3 Glitch.vst3"
            COMMAND codesign --force --deep --sign -
                    "${mp3glitch_staged_products}/Standalone/MP3 Glitch.app")
        if(TARGET MP3Glitch_AU)
            list(APPEND mp3glitch_artifact_commands
                COMMAND codesign --force --deep --sign -
                        "${mp3glitch_staged_products}/AU/MP3 Glitch.component")
        endif()
    endif()

    add_custom_target(MP3Glitch_Artifacts
        ${mp3glitch_artifact_commands}
        DEPENDS ${mp3glitch_artifact_dependencies}
        COMMENT "Staging MP3 Glitch products in ${MP3GLITCH_ARTIFACT_DIR}/$<CONFIG>")

    if(MP3GLITCH_BUILD_TESTS)
        enable_testing()
        add_executable(MP3GlitchIntegrationTests Tests/PluginIntegrationTest.cpp)
        target_include_directories(MP3GlitchIntegrationTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/MP3Glitch_artefacts/JuceLibraryCode")
        target_link_libraries(MP3GlitchIntegrationTests PRIVATE MP3Glitch juce::juce_audio_utils)
        add_dependencies(MP3GlitchIntegrationTests MP3Glitch_Artifacts)
        add_test(NAME MP3GlitchPluginIntegrationTests COMMAND MP3GlitchIntegrationTests)
        add_test(
            NAME MP3GlitchArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DMP3GLITCH_STAGED_DIR=${MP3GLITCH_ARTIFACT_DIR}/$<CONFIG>
                    -DMP3GLITCH_EXPECT_AU=$<BOOL:${APPLE}>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
elseif(MP3GLITCH_BUILD_TESTS)
    message(FATAL_ERROR "MP3GLITCH_BUILD_TESTS requires MP3GLITCH_BUILD_PLUGIN")
endif()
