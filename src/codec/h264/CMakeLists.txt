project(codec_h264)

aux_source_directory(${PROJECT_SOURCE_DIR} H264_FILES)

add_library(${PROJECT_NAME} ${H264_FILES})

target_link_libraries(${PROJECT_NAME} ${WEBRTC_LINK_LIBS} ${WEBRTC_LIBRARY})
