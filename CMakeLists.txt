cmake_minimum_required(VERSION 3.15)

# set(CMAKE_BUILD_TYPE Debug)

project(synth LANGUAGES C CXX VERSION 0.0.1)

# set(BUILD_SHARED_LIBS FALSE)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.10
#        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL2)

set(SDL2TTF_VENDORED OFF)
FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.24.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL2_ttf)

set(LIBREMIDI_NO_JACK ON)
if( ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  set(LIBREMIDI_EMSCRIPTEN ON)
endif()

FetchContent_Declare(
        libremidi
        GIT_REPOSITORY https://github.com/celtera/libremidi.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(libremidi)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)



set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)

set(IMGUI_BACKENDS
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_SOURCES} ${IMGUI_BACKENDS})
set_source_files_properties(${IMGUI_SOURCES} ${IMGUI_BACKENDS} PROPERTIES LANGUAGE CXX)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR})
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 libremidi::libremidi)

set(SOURCES
        src/app.c
        src/arpeggiator.c
        src/fx.c
        src/main.c
        src/midi.c
        src/mixer.c
        src/osc.c
        src/oscilloscope.c
        src/synth.c
        src/voice.c
        src/cJSON.c
        src/gui.cpp
)
add_executable(synth ${SOURCES})
set_source_files_properties(src/gui.cpp PROPERTIES LANGUAGE CXX)
install(TARGETS synth DESTINATION bin)
file(COPY "DejaVuSans.ttf" DESTINATION "${CMAKE_BINARY_DIR}")

find_package(OpenGL REQUIRED)

if( ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
  target_link_libraries(synth
    SDL2::SDL2main
    SDL2
    SDL2_ttf
    libremidi::libremidi
    imgui_lib
    m
  )

  set(USE_FLAGS "-sWASM=1 -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='[\"png\"]' -sUSE_SDL_TTF=2 -O3")
  set (CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${USE_FLAGS}")
  file(COPY "index.html" DESTINATION ${CMAKE_BINARY_DIR})
  file(COPY "server.py" DESTINATION ${CMAKE_BINARY_DIR})
else()
  target_link_libraries(synth
        PRIVATE
        SDL2::SDL2main
        SDL2
        SDL2_image
        SDL2_ttf
        libremidi::libremidi
        imgui_lib
        fftw3
        m
        OpenGL::GL
      )
endif()
