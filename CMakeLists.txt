cmake_minimum_required(VERSION 3.15)

# set(CMAKE_BUILD_TYPE Debug)

project(synth LANGUAGES C CXX VERSION 0.0.1)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

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

FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.32.10
#        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(SDL_STATIC_PIC ON)
set(SDL_STATIC ON)
set(SDL_TEST OFF)
set(SDL_TESTS OFF)
FetchContent_MakeAvailable(SDL2)

set(SDL2TTF_VENDORED TRUE)
if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()
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
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)

set(SOURCES
        src/adsr.c
        src/analog_filter.c
        src/app.c
        src/arpeggiator.c
        src/fx.c
        src/lfo.c
        src/main.c
        src/midi.c
        src/mixer.c
        src/osc.c
        src/oscilloscope.c
        src/ring_modulator.c
        src/synth.c
        src/utils.c
        src/voice.c
        src/cJSON.c
        src/gui.cpp
)
add_executable(synth ${SOURCES})
set_source_files_properties(src/gui.cpp PROPERTIES LANGUAGE CXX)
install(TARGETS synth DESTINATION bin)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
  target_link_libraries(synth
    SDL2::SDL2main
    SDL2-static
    SDL2_ttf
    libremidi::libremidi
    imgui_lib
    m
  )

  # Enable asyncify for emscripten_sleep support
  set(CMAKE_EXECUTABLE_SUFFIX ".js")
  set_target_properties(synth PROPERTIES
    SUFFIX ".js"
     LINK_FLAGS "-fPIC -s ASYNCIFY -s ASSERTIONS=1 -s ASYNCIFY_STACK_SIZE=5120000 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','JSEvents','Browser','HEAPU8'] -s EXPORTED_FUNCTIONS=['_main','_malloc','_free','_libremidi_devices_poll','libremidi_devices_input'] -s EXPORT_ALL -s FORCE_FILESYSTEM=0 -s NO_EXIT_RUNTIME=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s NO_DISABLE_EXCEPTION_CATCHING=1 -s ALLOW_MEMORY_GROWTH=1 -s STACK_SIZE=32MB -s INITIAL_MEMORY=512MB -s MAXIMUM_MEMORY=2GB -s SAFE_HEAP=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -O2 --pre-js pre.js"
  )
  
  set(CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz TRUE)
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
  find_package(OpenGL REQUIRED)
  target_link_libraries(synth
        PRIVATE
        SDL2::SDL2main
        SDL2
        SDL2_ttf
        libremidi::libremidi
        imgui_lib
        $<$<NOT:$<PLATFORM_ID:Windows>>:m>
        OpenGL::GL
       )
endif()

install(TARGETS synth
  RUNTIME ARCHIVE LIBRARY RUNTIME FRAMEWORK BUNDLE PUBLIC_HEADER RESOURCE)

if (WIN32)
  install(TARGETS synth
    COMPONENT synth
    RUNTIME_DEPENDENCIES
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
    DIRECTORIES $<TARGET_FILE_DIR:synth>
  )
  
  # Post-build step to copy DLL files to executable directory
  add_custom_command(TARGET synth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Finding and copying DLL files to executable directory..."
    COMMAND powershell -Command "Get-ChildItem -Path '${CMAKE_BINARY_DIR}' -Filter '*.dll' -Recurse | ForEach-Object { Copy-Item -Path $_.FullName -Destination '$<TARGET_FILE_DIR:synth>' -Force }"
    COMMENT "Copying all DLL files to executable directory"
  )
else()
  if(NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
    install(TARGETS synth
      RUNTIME_DEPENDENCIES
      POST_INCLUDE_FILES ${EXTRA_LIBRARY}
      RUNTIME DESTINATION .
      LIBRARY DESTINATION ./lib
      FRAMEWORK DESTINATION ./lib
    )
  endif()
endif()
