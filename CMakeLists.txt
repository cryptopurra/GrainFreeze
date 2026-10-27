cmake_minimum_required(VERSION 3.22)
project(GrainFreeze VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# GrainFreeze - an original granular + STFT "time freeze" effect (VST3/AU).
# Clean-room design. No third-party plugin code or resources are reused.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this at your JUCE checkout (git submodule or system install).
#   git clone https://github.com/juce-framework/JUCE extern/JUCE
add_subdirectory(extern/JUCE)

juce_add_plugin(GrainFreeze
    COMPANY_NAME        "YourName"
    BUNDLE_ID           "com.yourname.grainfreeze"
    PLUGIN_MANUFACTURER_CODE  Ynme          # 4 chars, exactly one uppercase
    PLUGIN_CODE               Grfz          # unique 4-char code
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "GrainFreeze"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE)

juce_generate_juce_header(GrainFreeze)

target_sources(GrainFreeze PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/CaptureBuffer.cpp
    Source/dsp/GrainEngine.cpp
    Source/dsp/SpectralEngine.cpp
    Source/dsp/StepModulator.cpp
    Source/dsp/StepSequencer.cpp
    Source/dsp/TempoDetector.cpp
    Source/dsp/Reducer.cpp)

target_compile_definitions(GrainFreeze PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GrainFreeze PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
