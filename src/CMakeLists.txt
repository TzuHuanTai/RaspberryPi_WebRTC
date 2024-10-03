project(src)

add_subdirectory(common)
add_subdirectory(track)
add_subdirectory(capturer)
add_subdirectory(v4l2_codecs)
add_subdirectory(signaling)
add_subdirectory(recorder)

add_library(${PROJECT_NAME}
    conductor.cpp
    customized_video_encoder_factory.cpp
    data_channel_subject.cpp
    parser.cpp
    rtc_peer.cpp
)

target_link_libraries(${PROJECT_NAME} PUBLIC track capturer v4l2_codecs signaling recorder common)
