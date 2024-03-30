cmake_minimum_required(VERSION 3.18)
project(pi_webrtc)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_TEST "" CACHE STRING "test")

if(USE_BUILT_IN_H264)
    add_definitions(-DUSE_BUILT_IN_H264)
    add_compile_definitions(WEBRTC_USE_H264)
endif()

if(USE_MQTT_SIGNALING)
    add_definitions(-DUSE_MQTT_SIGNALING)
elseif(USE_SIGNALR_SIGNALING)
    add_definitions(-DUSE_SIGNALR_SIGNALING)
endif()

set(WEBRTC_INCLUDE_DIR /usr/local/include/webrtc)
set(WEBRTC_LIBRARY /usr/local/lib/libwebrtc.a)
set(WEBRTC_LINK_LIBS X11 dl)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

message(STATUS "BUILD_TEST: ${BUILD_TEST}")
message(STATUS "WEBRTC_USE_H264: ${USE_BUILT_IN_H264}")
message(STATUS "WEBRTC_INCLUDE_DIR: ${WEBRTC_INCLUDE_DIR}")
message(STATUS "WEBRTC_LIBRARY: ${WEBRTC_LIBRARY}")

add_compile_definitions(
    NDEBUG
    WEBRTC_POSIX
    WEBRTC_LINUX
    USE_CPPRESTSDK
)

add_compile_options(
    -pipe
    -fdeclspec
    -g
)

include_directories(
    ./src
    ${WEBRTC_INCLUDE_DIR}
    ${WEBRTC_INCLUDE_DIR}/third_party/abseil-cpp
    ${WEBRTC_INCLUDE_DIR}/third_party/libyuv/include
    ${WEBRTC_INCLUDE_DIR}/tools/json_schema_compiler
)

if(BUILD_TEST STREQUAL "recorder")
    add_subdirectory(src/capture)
    add_subdirectory(src/recorder)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_recorder test/test_recorder.cpp)
    target_link_libraries(test_recorder
        capture
        recorder
        v4l2_codecs
    )
    target_link_libraries(test_recorder
        ${WEBRTC_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY}
    )
elseif(BUILD_TEST STREQUAL "pulseaudio")
    add_executable(test_pulseaudio test/test_pulseaudio.cpp)

    target_link_libraries(test_pulseaudio
        pulse-simple pulse
    )
elseif(BUILD_TEST STREQUAL "audio_recorder")
    add_subdirectory(src/recorder)
    add_subdirectory(src/v4l2_codecs)
    add_subdirectory(src/common)
    add_executable(test_audio_recorder test/test_audio_recorder.cpp)

    target_link_libraries(test_audio_recorder
        recorder
        Threads::Threads
        pulse-simple pulse avformat avcodec swscale avutil
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
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_v4l2_decoder test/test_v4l2_decoder.cpp)
    target_link_libraries(test_v4l2_decoder
        capture
        common
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
