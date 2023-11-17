project(src)

add_subdirectory(capture)
add_subdirectory(track)
add_subdirectory(v4l2_codecs)
add_subdirectory(signaling)

add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
    conductor.cpp
    customized_video_encoder_factory.cpp
    data_channel_subject.cpp
    parser.cpp
)

target_link_libraries(${PROJECT_NAME} capture track v4l2_codecs signaling)
