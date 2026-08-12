cmake_minimum_required(VERSION 3.22)

project(MP3Glitch VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MP3GLITCH_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(MP3GLITCH_BUILD_TESTS "Build plug-in integration tests" ON)
set(MP3GLITCH_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

if(MP3GLITCH_BUILD_PLUGIN)
    include(FetchContent)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    if(EHL_JUCE_SOURCE_DIR)
        if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
        endif()
        add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    else()
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.15
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    add_subdirectory(modules/juce-ehl-design-module)

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
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
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
            EHL::JuceDesign
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
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeVst3ModuleInfo.py"
                "${mp3glitch_staged_products}/VST3/MP3 Glitch.vst3/Contents/Resources/moduleinfo.json"
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
    set(ehl_stage_platform "linux-x64")
    if(APPLE)
        set(ehl_stage_platform "macos-arm64")
    elseif(WIN32)
        set(ehl_stage_platform "windows-x64")
    endif()
    set(ehl_stage_source_products "${MP3GLITCH_ARTIFACT_DIR}/$<CONFIG>")
    set(ehl_staged_products "${MP3GLITCH_ARTIFACT_DIR}/plugin-release/${ehl_stage_platform}")

    set(ehl_stage_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/au")

    if(APPLE)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/au/mp3glitch_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/MP3 Glitch.vst3" "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/Standalone/MP3 Glitch.app" "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/AU/MP3 Glitch.component" "${ehl_staged_products}/au/mp3glitch_au_plugin.component"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin.app"
            COMMAND codesign --force --deep --sign - "${ehl_staged_products}/au/mp3glitch_au_plugin.component")
    elseif(WIN32)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/MP3 Glitch.vst3" "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_stage_source_products}/Standalone/MP3 Glitch.exe" "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin.exe")
    else()
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/MP3 Glitch.vst3" "${ehl_staged_products}/vst3/mp3glitch_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_stage_source_products}/Standalone/MP3 Glitch" "${ehl_staged_products}/standalone/mp3glitch_standalone_plugin")
    endif()

    list(APPEND ehl_stage_commands
        COMMAND ${CMAKE_COMMAND}
            "-DPRODUCT_NAME=MP3 Glitch"
            -DSLUG=mp3glitch
            -DBUNDLE_ID=jp.ehl.mp3glitch
            -DPLUGIN_CODE=Mp3G
            -DSTAGE_DIR=${ehl_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${ehl_stage_commands}
        DEPENDS MP3Glitch_Artifacts
        COMMENT "Staging MP3 Glitch CI release products under ${MP3GLITCH_ARTIFACT_DIR}/plugin-release")


    if(MP3GLITCH_BUILD_TESTS)
        enable_testing()
        add_executable(MP3GlitchIntegrationTests Tests/PluginIntegrationTest.cpp)
        target_include_directories(MP3GlitchIntegrationTests PRIVATE
            Source
            "${CMAKE_CURRENT_BINARY_DIR}/MP3Glitch_artefacts/JuceLibraryCode")
        target_link_libraries(MP3GlitchIntegrationTests PRIVATE MP3Glitch juce::juce_audio_utils)
        add_dependencies(MP3GlitchIntegrationTests ehl_stage_products)
        add_test(NAME MP3GlitchPluginIntegrationTests COMMAND MP3GlitchIntegrationTests)
        add_test(
            NAME MP3GlitchArtifactChecks
            COMMAND ${CMAKE_COMMAND}
                    -DBUILD_CONFIG=$<CONFIG>
                    -DSTAGE_DIR=${ehl_staged_products}
                    -DSLUG=mp3glitch
                    -DEXPECT_AU=$<BOOL:${APPLE}>
                    -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
    endif()
elseif(MP3GLITCH_BUILD_TESTS)
    message(FATAL_ERROR "MP3GLITCH_BUILD_TESTS requires MP3GLITCH_BUILD_PLUGIN")
endif()
