# CMake configuration for test targets in source/tests

add_executable(rb26_challenger1_m2_tests
    Challenger1M2StressTests.cpp
)

target_include_directories(rb26_challenger1_m2_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rb26_challenger1_m2_tests PRIVATE
    rb26_dsp_core
)

set_target_properties(rb26_challenger1_m2_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_m3_stress_audit
    M3EmpiricalStressAudit.cpp
)

target_include_directories(rb26_m3_stress_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rb26_m3_stress_audit PRIVATE
    rb26_dsp_core
)

set_target_properties(rb26_m3_stress_audit PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_m4_tests
    M4JuceConcurrencyRoutingTests.cpp
)

target_include_directories(rb26_m4_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../plugin
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32 AND RB26_USE_WEBVIEW AND EXISTS "${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2.1.0.2849.39/build/native/include")
    target_include_directories(rb26_m4_tests PRIVATE
        ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2.1.0.2849.39/build/native/include
    )
endif()

target_link_libraries(rb26_m4_tests PRIVATE
    BRAUN_RB26
    juce::juce_gui_basics
    juce::juce_audio_processors
    juce::juce_audio_basics
)

set_target_properties(rb26_m4_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_diag_modal diag_modal.cpp)
target_include_directories(rb26_diag_modal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../dsp)
target_link_libraries(rb26_diag_modal PRIVATE rb26_dsp_core)
set_target_properties(rb26_diag_modal PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_hammer_transient_tests HammerTransientTests.cpp)
target_include_directories(rb26_hammer_transient_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../dsp)
target_link_libraries(rb26_hammer_transient_tests PRIVATE rb26_dsp_core)
set_target_properties(rb26_hammer_transient_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_diag_pitch diag_pitch.cpp)
target_include_directories(rb26_diag_pitch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../dsp)
target_link_libraries(rb26_diag_pitch PRIVATE rb26_dsp_core)
set_target_properties(rb26_diag_pitch PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rb26_diag_cpu diag_cpu_shimmer_dimmer.cpp)
target_include_directories(rb26_diag_cpu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../dsp)
target_link_libraries(rb26_diag_cpu PRIVATE rb26_dsp_core)
set_target_properties(rb26_diag_cpu PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)


