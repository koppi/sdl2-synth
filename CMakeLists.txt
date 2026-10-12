cmake_minimum_required(VERSION 3.15)

# set(CMAKE_BUILD_TYPE Debug)

# Version used for the plug-in metadata and release packages. Release builds
# override it from the git tag: cmake -DSYNTH_VERSION=1.2.3 ...
set(SYNTH_VERSION "0.1.0" CACHE STRING "Version (major.minor.patch) of the synth")
project(synth LANGUAGES C CXX VERSION ${SYNTH_VERSION})

# Dear ImGui and the UI code need at least C++11. Compilers differ in their
# default (AppleClang still defaults to C++98), so state it explicitly.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SYNTH_BUILD_STANDALONE "Build the standalone SDL2 application" ON)
option(SYNTH_BUILD_VST3 "Build the VST3 plug-in (downloads the Steinberg VST 3 SDK)" OFF)

if(EMSCRIPTEN)
  # The browser build only makes sense as the standalone app.
  set(SYNTH_BUILD_STANDALONE ON)
  set(SYNTH_BUILD_VST3 OFF)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# Speed up build for testing
set(FETCHCONTENT_QUIET TRUE)

# Increase WASM memory allocation for safety
if(EMSCRIPTEN)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=512MB")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s MAXIMUM_MEMORY=2GB")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s SAFE_HEAP=1")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s ABORTING_MALLOC=1")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s MAIN_MODULE=2")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# ---------------------------------------------------------------------------
# Dear ImGui (shared by the standalone app and the VST3 editor)
# ---------------------------------------------------------------------------
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)

# Core widgets only: no backends, so this links into a plug-in without pulling
# in SDL or OpenGL.
set(IMGUI_CORE_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
add_library(imgui_core STATIC ${IMGUI_CORE_SOURCES})
target_include_directories(imgui_core PUBLIC ${imgui_SOURCE_DIR})
# Keep ImGui symbols private so a plug-in never clashes with a host's own copy.
set_target_properties(imgui_core PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
# Synth core: the DSP engine, free of SDL / MIDI hardware / GUI dependencies
# ---------------------------------------------------------------------------
set(SYNTH_CORE_SOURCES
    src/adsr.c
    src/analog_filter.c
    src/arpeggiator.c
    src/cJSON.c
    src/fx.c
    src/lfo.c
    src/midi_map.c
    src/mixer.c
    src/osc.c
    src/ring_modulator.c
    src/synth.c
    src/utils.c
    src/voice.c
)
# Absolute paths so the plug-in subdirectory can reuse the list.
list(TRANSFORM SYNTH_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
add_library(synth_core STATIC ${SYNTH_CORE_SOURCES})
target_include_directories(synth_core PUBLIC src)
if(NOT WIN32)
  target_link_libraries(synth_core PUBLIC m)
endif()
set_target_properties(synth_core PROPERTIES C_VISIBILITY_PRESET hidden)

# The ImGui user interface itself (no platform or renderer backend)
add_library(synth_ui STATIC src/synth_ui.cpp)
target_link_libraries(synth_ui PUBLIC synth_core imgui_core)
set_target_properties(synth_ui PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

# ---------------------------------------------------------------------------
# Standalone SDL2 application
# ---------------------------------------------------------------------------
if(SYNTH_BUILD_STANDALONE)
  # Ship a single self-contained executable: link SDL and libremidi statically.
  set(SDL_SHARED OFF)
  set(SDL_STATIC ON)
  set(SDL_STATIC_PIC ON)
  set(SDL_TEST OFF)
  set(SDL_TESTS OFF)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
          SDL2
          GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
          GIT_TAG release-2.32.10
          GIT_SHALLOW TRUE
          GIT_PROGRESS TRUE
  )
  FetchContent_MakeAvailable(SDL2)

  set(LIBREMIDI_NO_JACK ON)
  if( ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(LIBREMIDI_EMSCRIPTEN ON)
  endif()

  FetchContent_Declare(
          libremidi
          GIT_REPOSITORY https://github.com/celtera/libremidi.git
          GIT_TAG v5.4.3
          GIT_SHALLOW TRUE
          GIT_PROGRESS TRUE
  )
  FetchContent_MakeAvailable(libremidi)

  set(IMGUI_BACKENDS
      ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )

  # ImGui plus the SDL2 / OpenGL3 backends used by the standalone app
  add_library(imgui_lib STATIC ${IMGUI_BACKENDS} ${imgui_SOURCE_DIR}/imgui_demo.cpp)
  set_source_files_properties(${IMGUI_BACKENDS} ${imgui_SOURCE_DIR}/imgui_demo.cpp PROPERTIES LANGUAGE CXX)
  target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui_lib PUBLIC imgui_core SDL2::SDL2)

  set(SOURCES
          src/app.c
          src/main.c
          src/midi.c
          src/gui.cpp
  )
  add_executable(synth ${SOURCES})
  set_source_files_properties(src/gui.cpp PROPERTIES LANGUAGE CXX)

  if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    target_link_libraries(synth
      SDL2::SDL2main
      SDL2-static
      libremidi::libremidi
      imgui_lib
      synth_ui
      synth_core
      m
    )

    # Enable asyncify for emscripten_sleep support
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    set_target_properties(synth PROPERTIES
      SUFFIX ".js"
       LINK_FLAGS "-fPIC -s ASYNCIFY -s ASSERTIONS=1 -s ASYNCIFY_STACK_SIZE=5120000 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','JSEvents','Browser','HEAPU8'] -s EXPORTED_FUNCTIONS=['_main','_malloc','_free','_libremidi_devices_poll','libremidi_devices_input'] -s EXPORT_ALL -s FORCE_FILESYSTEM=0 -s NO_EXIT_RUNTIME=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s NO_DISABLE_EXCEPTION_CATCHING=1 -s ALLOW_MEMORY_GROWTH=1 -s STACK_SIZE=32MB -s INITIAL_MEMORY=512MB -s MAXIMUM_MEMORY=2GB -s SAFE_HEAP=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -O2 --pre-js pre.js"
    )

    file(COPY "index.html" DESTINATION ${CMAKE_BINARY_DIR})

    add_custom_command(TARGET synth PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/pre.js
      ${CMAKE_BINARY_DIR}/pre.js
      COMMENT "Copying pre.js to build directory"
    )

    add_custom_command(TARGET synth POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/index.html
      ${CMAKE_BINARY_DIR}/index.html
      COMMENT "Copying index.html to build directory"
    )

    add_custom_command(TARGET synth POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/favicon.ico
      ${CMAKE_BINARY_DIR}/favicon.ico
      COMMENT "Copying favicon.ico to build directory"
    )
  else()
    # Link the classic libGL.so.1 (present on every desktop) rather than the
    # GLVND libOpenGL.so.0 so the released binary runs on more systems.
    set(OpenGL_GL_PREFERENCE LEGACY)
    find_package(OpenGL REQUIRED)
    target_link_libraries(synth
          PRIVATE
          SDL2::SDL2main
          SDL2::SDL2
          libremidi::libremidi
          imgui_lib
          synth_ui
          synth_core
          OpenGL::GL
         )
    install(TARGETS synth RUNTIME DESTINATION bin)
  endif()
endif()

# ---------------------------------------------------------------------------
# VST3 plug-in
# ---------------------------------------------------------------------------
if(SYNTH_BUILD_VST3)
  enable_testing() # ctest looks for the test file in the build root
  add_subdirectory(src/vst3)
endif()
