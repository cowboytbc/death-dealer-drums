cmake_minimum_required(VERSION 3.22)

project(DeathDealerDrums VERSION 1.0.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DDD_COPY_PLUGIN_AFTER_BUILD "Copy plugin builds into local DELIVERABLES folder after build" ON)
option(DDD_ENABLE_ASIO "Enable ASIO backend for Windows builds when ASIO SDK is available" ON)

set(DDD_ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/ASIOSDK" CACHE PATH "Path to Steinberg ASIO SDK root")
set(DDD_MACOS_ICNS "${CMAKE_SOURCE_DIR}/LOGO.icns" CACHE FILEPATH "Optional macOS .icns icon for standalone app bundle")

include(FetchContent)

FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(juce)

# ── Plugin target ─────────────────────────────────────────────────────────────

juce_add_plugin(DeathDealerDrums
    COMPANY_NAME                "INFERNO TONES"
    BUNDLE_ID                   "com.InfernoTones.DeathDealerDrums"
    PLUGIN_MANUFACTURER_CODE    Inft
    PLUGIN_CODE                 Dddr
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "DEATH DEALER DRUMS"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Instrument|Drums"
    PLUGIN_AU_SANDBOX_SAFE      TRUE
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/LOGO.png"
    ICON_SMALL                  "${CMAKE_SOURCE_DIR}/LOGO.png"
)

if(APPLE AND DEFINED DDD_MACOS_ICNS AND EXISTS "${DDD_MACOS_ICNS}")
    set_target_properties(DeathDealerDrums_Standalone PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "LOGO.icns"
    )
    target_sources(DeathDealerDrums_Standalone PRIVATE "${DDD_MACOS_ICNS}")
    set_source_files_properties("${DDD_MACOS_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

juce_generate_juce_header(DeathDealerDrums)

juce_add_binary_data(DeathDealerDrumsBinaryData SOURCES
    "NEW LOGO.png"
    "INFERNO TONES.png"
    DEFAULT.ddd
    DEMO.mid
    "SAMPLES/BELL.wav"
    "SAMPLES/CHINA.wav"
    "SAMPLES/CRASH 1.wav"
    "SAMPLES/CRASH 2.wav"
    "SAMPLES/HIHAT CLOSED.wav"
    "SAMPLES/HIHAT HALF LOOSE.wav"
    "SAMPLES/HIHAT LOOSE.wav"
    "SAMPLES/HIHAT OPEN.wav"
    "SAMPLES/KICK.wav"
    "SAMPLES/KICK 2.wav"
    "SAMPLES/RIDE.wav"
    "SAMPLES/RIM.wav"
    "SAMPLES/SNARE.wav"
    "SAMPLES/SPLASH.wav"
    "SAMPLES/TOM 1.wav"
    "SAMPLES/TOM 2.wav"
    "SAMPLES/TOM 3.wav"
    "SAMPLES/TOM 4.wav"
)

target_sources(DeathDealerDrums PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/DrumEngine.cpp
    source/DrumPad.cpp
)

target_compile_definitions(DeathDealerDrums
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

if(WIN32)
    target_compile_definitions(DeathDealerDrums PUBLIC
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
    )

    if(DDD_ENABLE_ASIO)
        if(EXISTS "${DDD_ASIO_SDK_DIR}/common/iasiodrv.h")
            target_compile_definitions(DeathDealerDrums PUBLIC JUCE_ASIO=1)
            target_include_directories(DeathDealerDrums PRIVATE
                "${DDD_ASIO_SDK_DIR}/common"
                "${DDD_ASIO_SDK_DIR}/host"
            )
            message(STATUS "ASIO enabled using SDK at: ${DDD_ASIO_SDK_DIR}")
        else()
            message(WARNING "DDD_ENABLE_ASIO is ON, but no ASIO SDK found at ${DDD_ASIO_SDK_DIR}. Build will continue without ASIO.")
        endif()
    endif()
endif()

target_link_libraries(DeathDealerDrums
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        DeathDealerDrumsBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Post-build copy ───────────────────────────────────────────────────────────

if(DDD_COPY_PLUGIN_AFTER_BUILD)
    set(DDD_DELIVERABLES_DIR "${CMAKE_SOURCE_DIR}/DELIVERABLES")

    if(WIN32)
        set(DDD_PLATFORM_DIR "${DDD_DELIVERABLES_DIR}/Windows")
    elseif(APPLE)
        set(DDD_PLATFORM_DIR "${DDD_DELIVERABLES_DIR}/macOS")
    else()
        set(DDD_PLATFORM_DIR "${DDD_DELIVERABLES_DIR}/Other")
    endif()

    add_custom_command(TARGET DeathDealerDrums POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DDD_PLATFORM_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:DeathDealerDrums>"
            "${DDD_PLATFORM_DIR}"
        COMMENT "Copying Death Dealer Drums plugin to DELIVERABLES"
    )
endif()
