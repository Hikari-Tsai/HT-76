cmake_minimum_required(VERSION 3.22)
project(HT-76 VERSION 0.9.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()
set(JUCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE" CACHE PATH "JUCE checkout")
if(EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850 GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(FIELD_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FIELD_FORMATS AU)
endif()
if(APPLE OR WIN32)
    # JUCE 8.0.12 includes the AAX SDK and builds its support library.
    list(APPEND FIELD_FORMATS AAX)
endif()
juce_add_binary_data(FieldEffectAssets HEADER_NAME BinaryData.h NAMESPACE BinaryData SOURCES
    design/assets/concept-knob-body.png
    design/assets/concept-metal.jpg
    design/assets/concept-vu-paper.jpg)
juce_add_plugin(FieldEffect
    COMPANY_NAME "Field Effect"
    BUNDLE_ID "com.hikaritsai.fieldeffect1176"
    PLUGIN_MANUFACTURER_CODE Hika
    PLUGIN_CODE Fe76
    PRODUCT_NAME "HT-76"
    DESCRIPTION "Stereo-linked FET limiting amplifier"
    FORMATS ${FIELD_FORMATS}
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio input is used to process live sound through the compressor."
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_IDENTIFIER "com.hikaritsai.fieldeffect1176"
    AAX_CATEGORY Dynamics
    VST3_CATEGORIES Fx Dynamics)
juce_generate_juce_header(FieldEffect)
file(GLOB FIELD_UI_SOURCES CONFIGURE_DEPENDS Source/PluginEditor.cpp Source/Panel*.cpp)
file(GLOB FIELD_DSP_SOURCES CONFIGURE_DEPENDS Source/DspEngine.cpp)
target_sources(FieldEffect PRIVATE Source/PluginProcessor.cpp ${FIELD_DSP_SOURCES} ${FIELD_UI_SOURCES})
target_include_directories(FieldEffect PUBLIC Source third_party/fetcomp-dsp
    "${CMAKE_CURRENT_BINARY_DIR}/FieldEffect_artefacts/JuceLibraryCode")
target_compile_definitions(FieldEffect PUBLIC
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(FieldEffect PRIVATE FieldEffectAssets juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
option(FIELD_BUILD_TESTS "Build signal tests and native snapshot tool" ON)
if(FIELD_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(FieldEffectTests PRODUCT_NAME "HT-76 Tests")
    # Shared plugin code defines this consistently for the console harness too.
    get_target_property(FIELD_TEST_DEFINITIONS FieldEffectTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM FIELD_TEST_DEFINITIONS "JUCE_STANDALONE_APPLICATION=1")
    set_target_properties(FieldEffectTests PROPERTIES COMPILE_DEFINITIONS "${FIELD_TEST_DEFINITIONS}")
    file(GLOB FIELD_DSP_TESTS CONFIGURE_DEPENDS Tests/DspTests.cpp)
    target_sources(FieldEffectTests PRIVATE Tests/TestMain.cpp Tests/ProcessorTests.cpp Tests/EditorTests.cpp ${FIELD_DSP_TESTS})
    target_link_libraries(FieldEffectTests PRIVATE FieldEffect FieldEffectAssets juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(FieldEffectTests PRIVATE FIELD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME FieldEffect.SignalAndState COMMAND FieldEffectTests)
endif()
