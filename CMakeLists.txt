cmake_minimum_required(VERSION 3.22)
project(NymphesController VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# NymphesController — preset browser/saver + CC controller for the Dreadbox
# Nymphes. One tree, one command:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
#   ctest --test-dir build --output-on-failure
#
# Targets
#   nymphes_core            the JUCE-free spine: preset codec, preset bank, CoreMIDI
#   nymphes-cli             headless preset browser / saver (no JUCE, no GUI)
#   nymphes-tests           codec + CC-map tests, no device, no audio hardware
#   nymphes-oracle          L4's independent codec, run as a cross-check oracle
#   NymphesController_VST3  the plugin
#   NymphesController_Standalone
#
# Ownership of the pieces, per fleet vote 6186a3295c47 and the codec votes:
#   preset/  midi/   L3  — codec, bank, CoreMIDI   (JUCE-free by design)
#   plugin/PluginProcessor.*  L3 — AudioProcessor
#   plugin/cc/  plugin/ui/  L1 — CC engine, editor, browser UI
#   core/            L4 — the shipping codec; preset/ is a facade over it
#   CMakeLists.txt  REPORT.md  install.sh  acceptance.sh  L1
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# --- codec selection ---------------------------------------------------------
# Two C++ codecs exist in this fleet and both votes required exactly ONE to ship.
# The plugin and the CLI are written against preset/ (L3), which also owns the
# PresetBank the browser lists, so that is the default. core/ (L4) is built as a
# separate executable and is never linked with the shipping codec, so the two
# cannot collide on nymphes::crc8 or on nymphes::Preset.
#
# -DNYMPHES_SHIPPED_CODEC=core     (default) core/ ships; preset/ is a facade
# -DNYMPHES_SHIPPED_CODEC=core     core/ ships instead, preset/ excluded
set(NYMPHES_SHIPPED_CODEC "core" CACHE STRING "Which C++ codec ships: preset or core")
set_property(CACHE NYMPHES_SHIPPED_CODEC PROPERTY STRINGS preset core)

# --- JUCE --------------------------------------------------------------------
# A local checkout via -DJUCE_DIR=/path/to/JUCE, otherwise the pinned release is
# fetched. Pinned, not "latest": the plug-in was built and checked against 8.0.12.
set(NYMPHES_JUCE_TAG "8.0.12")
if (DEFINED JUCE_DIR)
    if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found at JUCE_DIR='${JUCE_DIR}'")
    endif()
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${NYMPHES_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(juce)
    if (NOT juce_POPULATED)
        FetchContent_Populate(juce)
    endif()
    set(JUCE_DIR "${juce_SOURCE_DIR}")
endif()
message(STATUS "Nymphes: JUCE          ${JUCE_DIR}")
message(STATUS "Nymphes: shipped codec ${NYMPHES_SHIPPED_CODEC}")

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# =============================================================================
# Source collection. Globbed with CONFIGURE_DEPENDS so a lane adding a .cpp does
# not have to edit this file.
# =============================================================================

function(nymphes_glob out_var)
    set(_sources "")
    foreach (_dir ${ARGN})
        if (IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}")
            file(GLOB _found CONFIGURE_DEPENDS
                 "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/*.cpp"
                 "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/*.cc"
                 "${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/*.mm")
            list(APPEND _sources ${_found})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _sources)
    set(${out_var} ${_sources} PARENT_SCOPE)
endfunction()

# JUCE-free spine.
if (NYMPHES_SHIPPED_CODEC STREQUAL "core")
    # Adopted vote 9451d0f31582 option A (and 2ec8c18bf843 A before it): core/ is
    # the ONLY codec. preset/ may stay as a delegating facade (PresetBank + the
    # named-parameter view) but must carry no wire/CRC/protobuf logic of its own.
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/src/Codec.cpp")
        message(FATAL_ERROR
            "NYMPHES_SHIPPED_CODEC=core but core/src/Codec.cpp does not exist")
    endif()
    set(NYMPHES_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core/src/Codec.cpp")
    nymphes_glob(_facade preset)
    list(APPEND NYMPHES_CORE_SOURCES ${_facade})
    nymphes_glob(_midi midi)
    list(APPEND NYMPHES_CORE_SOURCES ${_midi})
    set(NYMPHES_CORE_EXTRA_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
    message(STATUS "Nymphes: codec = core/ (L4); preset/ is the delegating facade")
else()
    nymphes_glob(NYMPHES_CORE_SOURCES preset midi)
    set(NYMPHES_CORE_EXTRA_INCLUDES "")
endif()

# Plugin-side: the AudioProcessor shell, the CC engine and the editor.
nymphes_glob(NYMPHES_PLUGIN_SOURCES plugin plugin/ui plugin/cc)

# Front-ends / tests.
nymphes_glob(NYMPHES_CLI_SOURCES cli)
nymphes_glob(NYMPHES_TEST_SOURCES tests)

# The CC-map *view* (plugin/cc/CcSpec.cpp) is JUCE-free on purpose: the tests assert
# the table through it, so it belongs in the core library, not in the plugin. Nothing
# else under plugin/cc/ may be added here - the CC engine does use JUCE types.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/cc/CcSpec.cpp")
    list(APPEND NYMPHES_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plugin/cc/CcSpec.cpp")
endif()

foreach (_v NYMPHES_CORE_SOURCES NYMPHES_PLUGIN_SOURCES NYMPHES_CLI_SOURCES NYMPHES_TEST_SOURCES)
    list(LENGTH ${_v} _n)
    message(STATUS "Nymphes: ${_v} = ${_n} files")
endforeach()

# =============================================================================
# nymphes_core — JUCE-free. The CLI and the tests must build without JUCE, so
# nothing here may include a juce_* header.
# =============================================================================

if (NOT NYMPHES_CORE_SOURCES)
    message(FATAL_ERROR "nymphes_core has no sources: preset/ and midi/ are both empty")
endif()

# The CoreMIDI backend is Objective-C++; every other platform compiles its own
# backend (midi/MidiPortWin.cpp, or midi/MidiPortNone.cpp) instead.
if (NOT APPLE)
    list(FILTER NYMPHES_CORE_SOURCES EXCLUDE REGEX "\\.mm$")
endif()

add_library(nymphes_core STATIC ${NYMPHES_CORE_SOURCES})
target_include_directories(nymphes_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if (NYMPHES_CORE_EXTRA_INCLUDES)
    target_include_directories(nymphes_core PUBLIC ${NYMPHES_CORE_EXTRA_INCLUDES})
endif()
if (APPLE)
    target_link_libraries(nymphes_core PUBLIC
        "-framework CoreMIDI" "-framework CoreFoundation" "-framework CoreAudio")
elseif (WIN32)
    target_link_libraries(nymphes_core PUBLIC winmm)
endif()

# =============================================================================
# nymphes-cli — headless preset browser / saver. Plain executable, no JUCE.
# =============================================================================

if (NYMPHES_CLI_SOURCES)
    add_executable(nymphes-cli ${NYMPHES_CLI_SOURCES})
    target_link_libraries(nymphes-cli PRIVATE nymphes_core)
    set_target_properties(nymphes-cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
    message(WARNING "Nymphes: cli/ is empty — no nymphes-cli target")
endif()

# =============================================================================
# nymphes-tests — codec + CC map, no device, no audio hardware.
# =============================================================================

enable_testing()

# Each tests/*.cpp is its own executable with its own main(), registered as its own
# ctest entry. One file per concern keeps a failure attributable and lets a lane add
# a test without touching a shared main().
nymphes_glob(NYMPHES_TEST_SOURCES tests)
foreach (_test_src ${NYMPHES_TEST_SOURCES})
    get_filename_component (_test_name "${_test_src}" NAME_WE)
    add_executable(${_test_name} "${_test_src}")
    target_link_libraries(${_test_name} PRIVATE nymphes_core)
    target_include_directories(${_test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(${_test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    # The corpora live in the shared gate root, not in this tree, so the gate root
    # is handed to every test. This tree is <gate>/worktrees/integration, so the gate
    # is two levels up; -DNYMPHES_GATE_DIR=... overrides it.
    if (NOT DEFINED NYMPHES_GATE_DIR)
        get_filename_component(NYMPHES_GATE_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    endif()
    target_compile_definitions(${_test_name} PRIVATE
        NYMPHES_GATE_DIR="${NYMPHES_GATE_DIR}")

    add_test(NAME ${_test_name} COMMAND ${_test_name} "${NYMPHES_GATE_DIR}")
    set_tests_properties(${_test_name} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()

# =============================================================================
# nymphes-oracle — L4's independent codec, built as its own executable so it can
# never be linked into the shipping binaries alongside preset/. Its job is to
# disagree with the shipped codec if the shipped codec misreads the format.
# =============================================================================

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/src/Codec.cpp")
    add_library(nymphes_oracle_lib STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/core/src/Codec.cpp")
    target_include_directories(nymphes_oracle_lib PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/core/include")

    set(_oracle_main "")
    foreach (_cand core/tests/test_codec.cpp core/tests/main.cpp)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_cand}")
            list(APPEND _oracle_main "${CMAKE_CURRENT_SOURCE_DIR}/${_cand}")
        endif()
    endforeach()

    if (_oracle_main)
        add_executable(nymphes-oracle ${_oracle_main})
        target_link_libraries(nymphes-oracle PRIVATE nymphes_oracle_lib)
        set_target_properties(nymphes-oracle PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_test(NAME nymphes-oracle COMMAND nymphes-oracle "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
        set_tests_properties(nymphes-oracle PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()

# =============================================================================
# The plugin: VST3 + Standalone
# =============================================================================

if (NOT NYMPHES_PLUGIN_SOURCES)
    message(WARNING "Nymphes: plugin/ is empty — the VST3 target is not generated")
    return()
endif()

juce_add_plugin(NymphesController
    COMPANY_NAME              "Nymphes Fleet"
    BUNDLE_ID                 "com.nymphesfleet.nymphescontroller"
    PLUGIN_MANUFACTURER_CODE  Nymp
    PLUGIN_CODE               Nctr
    PRODUCT_NAME              "NymphesController"
    DESCRIPTION               "Dreadbox Nymphes preset browser and CC controller"
    FORMATS                   VST3 Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Instrument" "Fx"
    HARDENED_RUNTIME_ENABLED  TRUE)

target_sources(NymphesController PRIVATE ${NYMPHES_PLUGIN_SOURCES})
target_include_directories(NymphesController PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(NymphesController PRIVATE
    nymphes_core
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_dsp)
target_link_libraries(NymphesController PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
target_compile_definitions(NymphesController PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    NYMPHES_VERSION="${PROJECT_VERSION}")
juce_generate_juce_header(NymphesController)

# JUCE generates moduleinfo.json in a POST_BUILD step added by juce_add_plugin
# above, so this one runs after it: make the manifest strict JSON (hosts skip a
# plug-in whose manifest does not parse), then re-seal the bundle, whose ad-hoc
# signature covers the manifest.
get_target_property(NYMPHES_VST3_BUNDLE NymphesController_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET NymphesController_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DMODULEINFO=${NYMPHES_VST3_BUNDLE}/Contents/Resources/moduleinfo.json
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixModuleInfo.cmake
    VERBATIM)
if (APPLE)
    add_custom_command(TARGET NymphesController_VST3 POST_BUILD
        COMMAND codesign --force --sign - ${NYMPHES_VST3_BUNDLE}
        VERBATIM)
    # The linker signs only the standalone's executable; seal the whole .app so a
    # downloaded copy verifies as a bundle too.
    get_target_property(NYMPHES_APP_BUNDLE NymphesController_Standalone JUCE_PLUGIN_ARTEFACT_FILE)
    add_custom_command(TARGET NymphesController_Standalone POST_BUILD
        COMMAND codesign --force --sign - ${NYMPHES_APP_BUNDLE}
        VERBATIM)
endif()
