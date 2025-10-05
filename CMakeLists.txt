cmake_minimum_required(VERSION 3.24)

set(CMAKE_BUILD_TYPE Debug)

project(synth LANGUAGES C CXX VERSION 0.0.1)

# set(BUILD_SHARED_LIBS FALSE)

include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.8
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
        SDL2_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-2.8.8
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL2_image)

set(SDL2TTF_VENDORED OFF)
FetchContent_Declare(
        SDL2_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-2.24.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL2_ttf)

# set(LIBREMIDI_NO_JACK ON)
FetchContent_Declare(
        libremidi
        GIT_REPOSITORY https://github.com/celtera/libremidi.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(libremidi)

set(SOURCES
        src/app.c
        src/arpeggiator.c
        src/fx.c
        src/gui.c
        src/main.c
        src/midi.c
        src/mixer.c
        src/osc.c
        src/oscilloscope.c
        src/synth.c
        src/voice.c
        src/cJSON.c
        src/microui.c
)
add_executable(synth ${SOURCES})
install(TARGETS synth DESTINATION bin)

target_link_libraries(synth
        PRIVATE
        SDL2::SDL2main
        SDL2
        SDL2_image
        SDL2_ttf
        libremidi::libremidi
        fftw3
        m
)
