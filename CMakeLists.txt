cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# BRAUN RB-26 Studio Reverb — JUCE 8 CMake Multi-Platform & Multi-Format Build
# Supports: VST3, CLAP 1.0+, AU, AUv3, Standalone
# Platforms: Windows (x64/ARM64), macOS (Universal), Linux, iOS, Android
# ==============================================================================

# Configure Apple deployment target before project()
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()
endif()

project(BRAUN_RB26_PROJECT VERSION 1.3.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Real-Time Compiler Optimizations & Cross-Platform Warning Flags
# ==============================================================================
if(MSVC)
    add_compile_options(/utf-8 /W4 /O2 /fp:fast)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -ffast-math)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Ensure position independent code for audio plugin shared objects
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# ==============================================================================
# GUI Mode Selection Option: WebView (Default) vs Pure Native JUCE (Linux / Low Overhead)
# ==============================================================================
option(RB26_USE_WEBVIEW "Build BRAUN RB-26 with WebView GUI (disable for pure native JUCE UI)" ON)
if(DEFINED RB26_ENABLE_WEBVIEW)
    set(RB26_USE_WEBVIEW ${RB26_ENABLE_WEBVIEW})
endif()
if(DEFINED USE_WEBVIEW)
    set(RB26_USE_WEBVIEW ${USE_WEBVIEW})
endif()

option(RB26_BUILD_TESTS "Build RB-26 test suites" ON)

if(RB26_USE_WEBVIEW)
    message(STATUS "BRAUN RB-26 GUI Mode: Embedded Web Browser (WebView2/WebKit)")
    add_compile_definitions(
        JUCE_WEB_BROWSER=1
        RB26_USE_WEBVIEW=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
else()
    message(STATUS "BRAUN RB-26 GUI Mode: Pure Native JUCE (Dieter Rams LookAndFeel)")
    add_compile_definitions(
        JUCE_WEB_BROWSER=0
        RB26_USE_WEBVIEW=0
        JUCE_USE_WIN_WEBVIEW2=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()

# ==============================================================================
# System Dependencies (Linux libcurl for JUCE Web / Networking)
# ==============================================================================
if(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
endif()

include(FetchContent)

# ==============================================================================
# 1. Provision Microsoft.Web.WebView2 for Windows (Only if WebView enabled)
# ==============================================================================
if(WIN32 AND RB26_USE_WEBVIEW)
    set(WEBVIEW2_VERSION "1.0.2849.39")
    set(LOCAL_WEBVIEW2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
    set(BINARY_WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")

    if(EXISTS "${LOCAL_WEBVIEW2_DIR}/build/native/include/WebView2.h")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../build/packages" CACHE PATH "WebView2 location" FORCE)
    else()
        if(NOT EXISTS "${BINARY_WEBVIEW2_DIR}/build/native/include/WebView2.h")
            message(STATUS "Downloading Microsoft.Web.WebView2 NuGet package (${WEBVIEW2_VERSION})...")
            set(WEBVIEW2_ZIP "${CMAKE_BINARY_DIR}/webview2_${WEBVIEW2_VERSION}.zip")
            file(DOWNLOAD
                "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}"
                "${WEBVIEW2_ZIP}"
                SHOW_PROGRESS
            )
            file(ARCHIVE_EXTRACT
                INPUT "${WEBVIEW2_ZIP}"
                DESTINATION "${BINARY_WEBVIEW2_DIR}"
            )
            file(REMOVE "${WEBVIEW2_ZIP}")
        endif()
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages" CACHE PATH "WebView2 location" FORCE)
    endif()
endif()

# ==============================================================================
# 2. Fetch and Configure JUCE 8 (8.0.6)
# ==============================================================================
if(NOT TARGET juce::juce_audio_processors)
    # Check if JUCE source is cached locally in parent build/_deps
    set(LOCAL_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/juce-src")
    if(EXISTS "${LOCAL_JUCE_DIR}/CMakeLists.txt")
        message(STATUS "Using local JUCE 8 from ${LOCAL_JUCE_DIR}")
        FetchContent_Declare(
            JUCE
            SOURCE_DIR "${LOCAL_JUCE_DIR}"
        )
    else()
        message(STATUS "Fetching JUCE 8 (8.0.6)...")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.6
            GIT_SHALLOW    TRUE
        )
    endif()
    FetchContent_MakeAvailable(JUCE)
endif()

# ==============================================================================
# 3. Fetch CLAP-JUCE Extensions for Native CLAP 1.0+ Support
# ==============================================================================
if(NOT TARGET clap_juce_extensions)
    message(STATUS "Provisioning clap-juce-extensions...")
    FetchContent_Declare(
        clap-juce-extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        main
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_MakeAvailable(clap-juce-extensions)
endif()

# ==============================================================================
# 4. rb26_dsp_core Static Library (GUI-free, pure real-time C++20 DSP engine)
# ==============================================================================
add_library(rb26_dsp_core STATIC
    source/dsp/AcousticExciter.cpp
    source/dsp/AcousticExciter.h
    source/dsp/BoundedSaturator.cpp
    source/dsp/BoundedSaturator.h
    source/dsp/DspMath.h
    source/dsp/EarlyReflections.cpp
    source/dsp/EarlyReflections.h
    source/dsp/FdnReverbTank.cpp
    source/dsp/FdnReverbTank.h
    source/dsp/LowBandModalMatrix.cpp
    source/dsp/LowBandModalMatrix.h
    source/dsp/ManifoldDelayNetwork.cpp
    source/dsp/ManifoldDelayNetwork.h
    source/dsp/PitchShifter.cpp
    source/dsp/PitchShifter.h
    source/dsp/Rb26Engine.cpp
    source/dsp/Rb26Engine.h
    source/dsp/ShepardPitchSpiral.cpp
    source/dsp/ShepardPitchSpiral.h
    source/dsp/TailModulator.cpp
    source/dsp/TailModulator.h
)

target_include_directories(rb26_dsp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
)

target_link_libraries(rb26_dsp_core PUBLIC
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
    target_link_libraries(rb26_dsp_core PUBLIC CURL::libcurl)
endif()

# ==============================================================================
# 5. Packaging Web Assets (Zip Archive & Binary Data if web UI present and enabled)
# ==============================================================================
if(RB26_USE_WEBVIEW)
    set(RB26_WEB_DIR "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ui/index.html")
        set(RB26_WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
        set(RB26_WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web")
    endif()

    if(RB26_WEB_DIR)
        set(RB26_WEB_ASSETS_ZIP "${CMAKE_CURRENT_BINARY_DIR}/web_assets_rb26.zip")
        file(GLOB_RECURSE RB26_WEB_FILES "${RB26_WEB_DIR}/*")
        add_custom_command(
            OUTPUT "${RB26_WEB_ASSETS_ZIP}"
            COMMAND ${CMAKE_COMMAND} -E tar cf "${RB26_WEB_ASSETS_ZIP}" --format=zip .
            WORKING_DIRECTORY "${RB26_WEB_DIR}"
            DEPENDS ${RB26_WEB_FILES}
            COMMENT "Packaging RB-26 web assets into web_assets_rb26.zip"
        )
        add_custom_target(PackageRb26WebAssets DEPENDS "${RB26_WEB_ASSETS_ZIP}")
        juce_add_binary_data(BraunRb26WebAssets SOURCES "${RB26_WEB_ASSETS_ZIP}")
        add_dependencies(BraunRb26WebAssets PackageRb26WebAssets)
    endif()
endif()

# ==============================================================================
# 6. Target Plugin Formats & BRAUN_RB26 Plugin Declaration
# ==============================================================================
set(RB26_FORMATS VST3 Standalone)
if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(RB26_FORMATS AUv3 Standalone)
    else()
        list(APPEND RB26_FORMATS AU AUv3)
    endif()
endif()

set(RB26_EXTRA_PLUGIN_ARGS "")
if(RB26_USE_WEBVIEW)
    if(WIN32)
        list(APPEND RB26_EXTRA_PLUGIN_ARGS NEEDS_WEBVIEW2 TRUE)
    endif()
    if(UNIX AND NOT APPLE)
        list(APPEND RB26_EXTRA_PLUGIN_ARGS NEEDS_WEB_BROWSER TRUE)
    endif()
endif()

juce_add_plugin(BRAUN_RB26
    COMPANY_NAME "Braun"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Brun
    PLUGIN_CODE Rb26
    FORMATS ${RB26_FORMATS}
    PRODUCT_NAME "BRAUN_RB26"
    VERSION "1.3.6"
    ${RB26_EXTRA_PLUGIN_ARGS}
)

target_sources(BRAUN_RB26 PRIVATE
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginProcessor.h
    source/plugin/PluginEditor.cpp
    source/plugin/PluginEditor.h
    source/plugin/Parameters.h
    source/plugin/LookAndFeel/BraunLookAndFeel.cpp
    source/plugin/LookAndFeel/BraunLookAndFeel.h
)

target_include_directories(BRAUN_RB26 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/LookAndFeel
    ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
)

if(RB26_USE_WEBVIEW)
    target_compile_definitions(BRAUN_RB26 PUBLIC
        JUCE_WEB_BROWSER=1
        RB26_USE_WEBVIEW=1
        JUCE_USE_WIN_WEBVIEW2=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(BRAUN_RB26 PRIVATE CURL::libcurl)
    endif()
else()
    target_compile_definitions(BRAUN_RB26 PUBLIC
        JUCE_WEB_BROWSER=0
        RB26_USE_WEBVIEW=0
        JUCE_USE_WIN_WEBVIEW2=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()

target_link_libraries(BRAUN_RB26 PRIVATE
    rb26_dsp_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(TARGET BraunRb26WebAssets)
    target_link_libraries(BRAUN_RB26 PRIVATE BraunRb26WebAssets)
endif()

# Attach CLAP format target via clap-juce-extensions
if(TARGET clap_juce_extensions)
    clap_juce_extensions_plugin(
        TARGET BRAUN_RB26
        CLAP_ID "com.braun.rb26"
        CLAP_FEATURES audio-effect reverb space spatial
    )
endif()

# ==============================================================================
# 7. Headless Continuous Verification Test Runner
# ==============================================================================
if(RB26_BUILD_TESTS)
    enable_testing()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/tests/rb26_headless_dsp_tests.cpp")
        add_executable(rb26_headless_dsp_tests
            source/tests/rb26_headless_dsp_tests.cpp
        )
        target_link_libraries(rb26_headless_dsp_tests PRIVATE
            rb26_dsp_core
        )
        target_include_directories(rb26_headless_dsp_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source
            ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
            ${CMAKE_CURRENT_SOURCE_DIR}/source/tests
        )
        set_target_properties(rb26_headless_dsp_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
        add_test(NAME Rb26HeadlessDspTests COMMAND rb26_headless_dsp_tests)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/tests/Challenger2Tests.cpp")
        add_executable(rb26_dsp_tests
            source/tests/Challenger2Tests.cpp
        )
        target_include_directories(rb26_dsp_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source
            ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp
        )
        target_link_libraries(rb26_dsp_tests PRIVATE
            rb26_dsp_core
        )
        add_test(NAME Rb26DspChallengerTests COMMAND rb26_dsp_tests)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/tests/BraunLookAndFeelTests.cpp")
        add_executable(rb26_laf_tests
            source/tests/BraunLookAndFeelTests.cpp
            source/plugin/LookAndFeel/BraunLookAndFeel.cpp
        )
        target_compile_definitions(rb26_laf_tests PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
        )
        target_include_directories(rb26_laf_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source
            ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin
            ${CMAKE_CURRENT_SOURCE_DIR}/source/plugin/LookAndFeel
        )
        target_link_libraries(rb26_laf_tests PRIVATE
            juce::juce_gui_basics
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
        )
        if(UNIX AND NOT APPLE)
            target_link_libraries(rb26_laf_tests PRIVATE CURL::libcurl)
        endif()
        add_test(NAME Rb26LookAndFeelTests COMMAND rb26_laf_tests)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/tests/WebResourceTests.cpp" AND TARGET BraunRb26WebAssets)
        add_executable(rb26_web_resource_tests
            source/tests/WebResourceTests.cpp
        )
        target_include_directories(rb26_web_resource_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source
        )
        target_link_libraries(rb26_web_resource_tests PRIVATE
            BraunRb26WebAssets
            juce::juce_core
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
        )
        if(UNIX AND NOT APPLE)
            target_link_libraries(rb26_web_resource_tests PRIVATE CURL::libcurl)
        endif()
        add_test(NAME Rb26WebResourceTests COMMAND rb26_web_resource_tests)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/source/tests/CMakeLists.txt")
        add_subdirectory(source/tests)
    endif()
endif()
