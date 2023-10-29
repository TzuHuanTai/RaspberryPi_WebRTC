cmake_minimum_required(VERSION 3.18)
project(pi_webrtc)

set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(BUILD_TEST "" CACHE STRING "test")
set(WEBRTC_INCLUDE_DIR /usr/local/include/webrtc)
set(WEBRTC_LIBRARY_DIR /usr/local/lib/libwebrtc.a)
set(WEBRTC_LINK_LIBS X11 dl)
set(SIGNALR_LINK_LIBS microsoft-signalr cpprest ssl crypto)
set(FFMPEG_LINK_LIBS avformat avcodec avutil swscale avdevice)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
message(STATUS "BUILD_TEST: ${BUILD_TEST}")
message(STATUS "WEBRTC_INCLUDE_DIR: ${WEBRTC_INCLUDE_DIR}")
message(STATUS "WEBRTC_LIBRARY_DIR: ${WEBRTC_LIBRARY_DIR}")

add_compile_definitions(
    WEBRTC_POSIX
    WEBRTC_LINUX
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

add_library(common)
target_sources(common
    PRIVATE
    src/common/v4l2_utils.cpp
    src/common/recorder.cpp
    src/common/worker.cpp
)

if(BUILD_TEST STREQUAL "recorder")
    add_subdirectory(src/capture)
    add_executable(test_recorder test/test_recorder.cpp)
    target_link_libraries(test_recorder
        capture
        common
    )
    target_link_libraries(test_recorder
        ${WEBRTC_LINK_LIBS}
        ${FFMPEG_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY_DIR}
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
        ${WEBRTC_LIBRARY_DIR}
    )
elseif(BUILD_TEST STREQUAL "v4l2_encoder")
    add_subdirectory(src/capture)
    add_subdirectory(src/v4l2_codecs)
    add_executable(test_v4l2_encoder test/test_v4l2_encoder.cpp)
    target_link_libraries(test_v4l2_encoder
        capture
        common
        v4l2_codecs
    )
    target_link_libraries(test_v4l2_encoder
        ${WEBRTC_LINK_LIBS}
        ${FFMPEG_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY_DIR}
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
        ${FFMPEG_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY_DIR}
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
        ${FFMPEG_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY_DIR}
    )
else()
    add_subdirectory(src)
    add_executable(${PROJECT_NAME} src/main.cpp)
    target_link_libraries(${PROJECT_NAME}
        src
        common
        capture
        track
        v4l2_codecs
    )
    target_link_libraries(${PROJECT_NAME}
        ${WEBRTC_LINK_LIBS}
        ${SIGNALR_LINK_LIBS}
        boost_program_options
        ${FFMPEG_LINK_LIBS}
        Threads::Threads
        ${WEBRTC_LIBRARY_DIR}
    )
endif()
