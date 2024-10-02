cmake_minimum_required(VERSION 3.18)
project(pi_webrtc)

set(CMAKE_CXX_STANDARD 20)
set(BUILD_TEST "" CACHE STRING "test")

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)
if(NOT BUILD_TYPE_LOWER OR BUILD_TYPE_LOWER STREQUAL "debug")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
    add_definitions(-DDEBUG_MODE)
    find_library(PROFILER_LIB profiler)
    if(PROFILER_LIB)
        message(STATUS "Profiler library found: ${PROFILER_LIB}")
        add_link_options(-lprofiler)
    else()
        message(STATUS "Profiler library not found")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

option(USE_MQTT_SIGNALING "Enable MQTT signaling" ON)
if(USE_MQTT_SIGNALING)
    add_definitions(-DUSE_MQTT_SIGNALING)
    message(STATUS "SIGNALING: MQTT")
endif()

set(WEBRTC_INCLUDE_DIR /usr/local/include/webrtc)
set(WEBRTC_LIBRARY /usr/local/lib/libwebrtc.a)
set(WEBRTC_LINK_LIBS dl)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

message(STATUS "BUILD_TEST: ${BUILD_TEST}")
message(STATUS "WEBRTC_INCLUDE_DIR: ${WEBRTC_INCLUDE_DIR}")
message(STATUS "WEBRTC_LIBRARY: ${WEBRTC_LIBRARY}")

add_compile_definitions(
    NDEBUG
    WEBRTC_POSIX
    WEBRTC_LINUX
    WEBRTC_USE_H264
    USE_CPPRESTSDK
)

add_compile_options(
    -pipe
    -fdeclspec
)

include_directories(
    ./src
    ${WEBRTC_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
    ${WEBRTC_INCLUDE_DIR}/tools/json_schema_compiler
)

if(BUILD_TEST STREQUAL "pulseaudio")
    add_executable(test_pulseaudio test/test_pulseaudio.cpp)

    target_link_libraries(test_pulseaudio
        pulse-simple pulse
    )
elseif(BUILD_TEST STREQUAL "recorder")
    add_subdirectory(src/common)
    add_subdirectory(src/capture)
    add_subdirectory(src/recorder)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_recorder test/test_recorder.cpp)

    target_link_libraries(test_recorder
        capture
        common
        recorder
        Threads::Threads
        ${WEBRTC_LINK_LIBS}
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "mqtt")
    add_subdirectory(src/signaling)
    add_executable(test_mqtt test/test_mqtt.cpp)

    target_link_libraries(test_mqtt
        signaling
    )
    target_link_libraries(test_mqtt
        Threads::Threads
        ${MOSQUITTO_LIBS}
    )
elseif(BUILD_TEST STREQUAL "openh264")
    add_executable(test_openh264 test/test_openh264.cpp)
    target_link_libraries(test_openh264
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "v4l2_capture")
    add_subdirectory(src/capture)
    add_executable(test_v4l2_capture test/test_v4l2_capture.cpp)
    target_link_libraries(test_v4l2_capture
        capture
        common
    )
    target_link_libraries(test_v4l2_capture
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "v4l2_encoder")
    add_subdirectory(src/capture)
    add_subdirectory(src/common)
    add_subdirectory(src/recorder)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_v4l2_encoder test/test_v4l2_encoder.cpp)
    target_link_libraries(test_v4l2_encoder
        capture
        recorder
        v4l2_codecs
    )
    target_link_libraries(test_v4l2_encoder
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "v4l2_decoder")
    add_subdirectory(src/capture)
    add_subdirectory(src/common)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_v4l2_decoder test/test_v4l2_decoder.cpp)
    target_link_libraries(test_v4l2_decoder
        capture
        v4l2_codecs
    )
    target_link_libraries(test_v4l2_decoder
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "v4l2_scaler")
    add_subdirectory(src/capture)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_v4l2_scaler test/test_v4l2_scaler.cpp)
    target_link_libraries(test_v4l2_scaler
        capture
        common
        v4l2_codecs
    )
    target_link_libraries(test_v4l2_scaler
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "libcamera")
    add_subdirectory(src/capture)
    add_subdirectory(src/common)
    add_executable(test_libcamera test/test_libcamera.cpp)
    target_link_libraries(test_libcamera
        capture
        common
    )
    target_link_libraries(test_libcamera
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
else()
    add_subdirectory(src)
    add_executable(${PROJECT_NAME} src/main.cpp)
    target_link_libraries(${PROJECT_NAME}
        src
    )

    target_link_libraries(${PROJECT_NAME}
        ${WEBRTC_LINK_LIBS}
        boost_program_options
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
endif()
