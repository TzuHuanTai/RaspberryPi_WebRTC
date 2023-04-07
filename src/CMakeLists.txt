project(src)

add_library(common)
target_sources(common
    PRIVATE
    v4l2_utils.cpp
    recorder.cpp
)

add_subdirectory(encoder)
add_subdirectory(capture)

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
    conductor.cpp
    customized_video_encoder_factory.cpp
    data_channel_subject.cpp
    parser.cpp
    signal_server.cpp
    v4l2_capture.cpp
    h264_capture.cpp
)

target_link_libraries(${PROJECT_NAME} encoder capture)
