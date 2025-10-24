cmake_minimum_required(VERSION 3.14)

# ===== Common baseline =====
project(SineSynth VERSION 1.0.0.0 DESCRIPTION "SineSynth VST 3 Plug-in")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()

# For Linux shared objects: keep rpath relative
if(UNIX AND NOT APPLE)
    set(CMAKE_SKIP_RPATH OFF)
    set(CMAKE_BUILD_RPATH "\$ORIGIN")
    set(CMAKE_INSTALL_RPATH "\$ORIGIN")
endif()

# ===== Read .env to env & cache vars (your block kept, just wrapped) =====
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.env" DOTENV_RAW)
    string(REPLACE "\r\n" "\n" DOTENV_RAW "${DOTENV_RAW}")
    string(REPLACE "\r"   "\n" DOTENV_RAW "${DOTENV_RAW}")
    string(REGEX REPLACE "\n$" "" DOTENV_RAW "${DOTENV_RAW}")
    string(REPLACE "\n" ";" DOTENV_LINES "${DOTENV_RAW}")
    foreach(line IN LISTS DOTENV_LINES)
        string(REGEX MATCH "([A-Za-z_][A-Za-z0-9_]*)=(.*)" _ "${line}")
        if (CMAKE_MATCH_1 AND CMAKE_MATCH_2)
            set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
            # Also cache as CMake var if not set
            if(NOT DEFINED ${CMAKE_MATCH_1})
                set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE STRING "Loaded from .env" FORCE)
            endif()
        endif()
    endforeach()
endif()

# ===== Options =====
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
option(INSTALL_TO_HOME_VST3 "Copy plugin to ~/.vst3 on Linux" OFF)

# Headless by default on Linux containers (override with -DHEADLESS=OFF)
if(UNIX AND NOT APPLE)
    option(HEADLESS "Build without VSTGUI/X11 deps" ON)
else()
    option(HEADLESS "Build without VSTGUI/X11 deps" OFF)
endif()

# Optional: copy or symlink to user's VST3 folder after build
set(USE_SYMLINK "$ENV{USE_SYMLINK}")
if(USE_SYMLINK STREQUAL "")
    set(USE_SYMLINK "ON")
endif()

# ===== Resolve paths from .env =====
set(vst3sdk_SOURCE_DIR "${VST_PATH}")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "VST_PATH is empty! Put VST3 SDK path into .env as VST_PATH=/path/to/vst3sdk")
endif()

# Expand ~ in SYMLINK_PATH if present
if(SYMLINK_PATH MATCHES "^~")
    string(REGEX REPLACE "^~" "$ENV{HOME}" SYMLINK_PATH "${SYMLINK_PATH}")
endif()

message(STATUS "VST_PATH: ${vst3sdk_SOURCE_DIR}")
message(STATUS "SYMLINK_PATH: ${SYMLINK_PATH}")
message(STATUS "HEADLESS: ${HEADLESS}")
message(STATUS "INSTALL_TO_HOME_VST3: ${INSTALL_TO_HOME_VST3}")
message(STATUS "USE_SYMLINK: ${USE_SYMLINK}")

# ===== SDK add & setup =====
# If you build VSTGUI from SDK root, keep this
set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")
set(SMTG_PLUGIN_TARGET_PATH "${CMAKE_CURRENT_BINARY_DIR}/VST3/${CMAKE_BUILD_TYPE}" CACHE PATH "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)

# Линкуем Steinberg SDK статически
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Единый ABI (современный)
add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

# ===== Linux GUI deps if not headless =====
if(UNIX AND NOT APPLE AND NOT HEADLESS)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    # Optional: ALSA/JACK if вы используете аудио I/O в standalone/host
    find_path(ALSA_INCLUDE_DIR NAMES alsa/asoundlib.h)
    find_library(ALSA_LIBRARY asound)
    find_library(JACK_LIBRARY jack)
endif()

# ===== Target sources =====
smtg_add_vst3plugin(SineSynth
    source/version.h
    source/cids.h
    source/processor.h
    source/processor.cpp
    source/controller.h
    source/controller.cpp
    source/entry.cpp
    source/Env.h
    source/Log.h
    source/SynthParameter.h
    source/ADSR.h
    source/ADSR.cpp
    source/SynthVoice.h
    source/Synth.h
    source/Synth.cpp
)

if(HEADLESS)
    target_compile_definitions(SineSynth PRIVATE
        SMTG_ENABLE_VSTGUI_SUPPORT=0
        NO_VSTGUI=1
    )
endif()

target_link_libraries(SineSynth PRIVATE sdk)

add_library(VstHostCapi SHARED host/VstHostCapi.cpp)
target_include_directories(VstHostCapi PRIVATE
    ${vst3sdk_SOURCE_DIR}
    ${vst3sdk_SOURCE_DIR}/public.sdk/source
    ${CMAKE_CURRENT_SOURCE_DIR}/host
)

set(SDK_HOSTING_DIR "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting")
if(EXISTS "${SDK_HOSTING_DIR}/module_linux.cpp")
  set(MODULE_CPP "${SDK_HOSTING_DIR}/module_linux.cpp")
else()
  set(MODULE_CPP "${SDK_HOSTING_DIR}/module.cpp")
endif()

target_sources(VstHostCapi PRIVATE
  ${MODULE_CPP}
  ${SDK_HOSTING_DIR}/parameterchanges.cpp
  ${SDK_HOSTING_DIR}/eventlist.cpp
)

target_link_libraries(VstHostCapi PRIVATE sdk_hosting sdk base pluginterfaces)

# видимость по умолчанию + не прятать инлайны
set_target_properties(VstHostCapi PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME "VstHostCapi"
)

if(UNIX AND NOT APPLE)
  target_link_options(VstHostCapi PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/host/exports.map"
    "-Wl,--no-undefined"
  )
endif()

# ===== VSTGUI resources (only if enabled) =====
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(SineSynth PRIVATE resource/editor.uidesc)
    target_link_libraries(SineSynth PRIVATE vstgui_support)
    smtg_target_add_plugin_resources(SineSynth RESOURCES "resource/editor.uidesc")
endif()

# ===== Snapshots =====
smtg_target_add_plugin_snapshots(SineSynth
    RESOURCES
        resource/072865FDCC115CBDB698A7D61F3767D2_snapshot.png
        resource/072865FDCC115CBDB698A7D61F3767D2_snapshot_2.0x.png
)

# ===== Version file =====
smtg_target_configure_version_file(SineSynth)

# ===== Per-OS tweaks =====
if(SMTG_MAC)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")
    smtg_target_set_bundle(SineSynth
        BUNDLE_IDENTIFIER com.radiyx.sine-synth
        COMPANY_NAME "RadiyX"
    )
    smtg_target_set_debug_executable(SineSynth
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(SineSynth PRIVATE resource/win32resource.rc)
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SineSynth)
        smtg_target_set_debug_executable(SineSynth
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
elseif(UNIX AND NOT APPLE)
    # ничего не делаем для Linux — выкладкой и путями управляет docker-compose
endif()
