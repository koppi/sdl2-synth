# Smoke test host for the VST3 plug-in. Added from src/vst3/CMakeLists.txt.

set(TEST_HOST_SOURCES
    test_host.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/plugprovider.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/moduleinfo/moduleinfoparser.cpp
)
if(APPLE)
  list(APPEND TEST_HOST_SOURCES ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_mac.mm)
  set_source_files_properties(${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_mac.mm
                              PROPERTIES COMPILE_FLAGS "-fobjc-arc")
elseif(WIN32)
  list(APPEND TEST_HOST_SOURCES ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp)
else()
  list(APPEND TEST_HOST_SOURCES ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_linux.cpp)
endif()

add_executable(vst3_test_host ${TEST_HOST_SOURCES})
target_compile_features(vst3_test_host PRIVATE cxx_std_17)
target_link_libraries(vst3_test_host PRIVATE sdk_hosting)
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(vst3_test_host PRIVATE X11::X11 ${CMAKE_DL_LIBS})
elseif(APPLE)
  target_link_libraries(vst3_test_host PRIVATE "-framework Cocoa")
endif()
smtg_target_setup_universal_binary(vst3_test_host)
add_dependencies(vst3_test_host ${VST3_TARGET})

# ctest runs the smoke test against the freshly built plug-in bundle.
get_target_property(SYNTH_VST3_PACKAGE ${VST3_TARGET} SMTG_PLUGIN_PACKAGE_PATH)
add_test(NAME vst3-smoke COMMAND vst3_test_host "${SYNTH_VST3_PACKAGE}")
if(WIN32 OR APPLE)
  set_tests_properties(vst3-smoke PROPERTIES SKIP_REGULAR_EXPRESSION ".*" DISABLED TRUE)
endif()
