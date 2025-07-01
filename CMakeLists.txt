cmake_minimum_required(VERSION 3.17)

project(synth LANGUAGES C VERSION 0.0.1)

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
)
add_executable(synth ${SOURCES})

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(rtmidi CONFIG REQUIRED)

target_link_libraries(synth
        PRIVATE
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
        $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
        $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
        winmm RtMidi::rtmidi
)

if (WIN32)
    add_compile_definitions(__WINDOWS_MM__)
endif()