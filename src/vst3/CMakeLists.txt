# VST3 plug-in. Included from the top-level CMakeLists.txt when
# SYNTH_BUILD_VST3 is ON.

if(CMAKE_VERSION VERSION_LESS 3.25)
  message(FATAL_ERROR "Building the VST3 plug-in requires CMake 3.25 or newer (found ${CMAKE_VERSION}).")
endif()

# --- Steinberg VST 3 SDK (MIT licensed since 3.8) ---------------------------
# Only the parts needed to build a plug-in are fetched; the SDK's samples and
# the VSTGUI toolkit are not used.
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
# Do not touch the user's plug-in folder as a side effect of building.
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Symlink the built plug-in into the user's VST3 folder")
if(MSVC)
  # Ship a plug-in that does not need the Visual C++ redistributable.
  set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.1_build_84
    GIT_SHALLOW TRUE
    GIT_SUBMODULES base cmake pluginterfaces public.sdk
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(vst3sdk)

smtg_enable_vst3_sdk()

# --- Plug-in target ------------------------------------------------------------
set(VST3_TARGET sdl2-synth-vst3)

# Native window glue for the editor, one implementation per platform.
if(APPLE)
  set(VST3_PLATFORM_SOURCES platform_cocoa.mm)
elseif(WIN32)
  set(VST3_PLATFORM_SOURCES platform_win32.cpp)
else()
  set(VST3_PLATFORM_SOURCES platform_x11.cpp)
endif()

set(VST3_SOURCES
    entry.cpp
    controller.cpp
    controller.h
    editor.cpp
    editor.h
    editor_core.cpp
    editor_core.h
    platform.h
    sw_renderer.cpp
    sw_renderer.h
    ids.h
    params.cpp
    params.h
    processor.cpp
    processor.h
    viz.cpp
    viz.h
    fpu.h
    ${VST3_PLATFORM_SOURCES}
)

smtg_add_vst3plugin(${VST3_TARGET}
    PACKAGE_NAME "sdl2-synth"
    SOURCES_LIST
        ${VST3_SOURCES}
        # The engine and the shared ImGui UI are compiled straight into the
        # plug-in so it is one self-contained module.
        ${CMAKE_SOURCE_DIR}/src/synth_ui.cpp
        ${SYNTH_CORE_SOURCES}
        ${IMGUI_CORE_SOURCES}
)

target_include_directories(${VST3_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
)
target_compile_features(${VST3_TARGET} PRIVATE cxx_std_17)
target_compile_definitions(${VST3_TARGET} PRIVATE SDL2SYNTH_VERSION_STR="${PROJECT_VERSION}")
target_link_libraries(${VST3_TARGET} PRIVATE sdk)
set_target_properties(${VST3_TARGET} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
if(NOT WIN32)
  target_link_libraries(${VST3_TARGET} PRIVATE m)
endif()
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(${VST3_TARGET} PRIVATE X11::X11)
  target_link_options(${VST3_TARGET} PRIVATE -Wl,--exclude-libs,ALL)
elseif(WIN32)
  target_link_libraries(${VST3_TARGET} PRIVATE user32 gdi32)
elseif(APPLE)
  set_source_files_properties(platform_cocoa.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  target_link_libraries(${VST3_TARGET} PRIVATE "-framework Cocoa" "-framework QuartzCore")
endif()

if(APPLE)
  smtg_target_set_bundle(${VST3_TARGET}
      BUNDLE_IDENTIFIER "io.github.koppi.sdl2-synth"
      COMPANY_NAME "koppi"
  )
endif()

# --- Smoke test host (ctest) -------------------------------------------------------
option(SYNTH_BUILD_VST3_TESTS "Build the VST3 smoke test host" ON)
if(SYNTH_BUILD_VST3_TESTS)
  add_subdirectory(${CMAKE_SOURCE_DIR}/tests/vst3 ${CMAKE_BINARY_DIR}/tests/vst3)
endif()
