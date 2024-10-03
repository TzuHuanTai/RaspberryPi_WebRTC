project(recorder)

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/src/codec/h264
    ${CMAKE_BINARY_DIR}/src/codec/h264
)

set(FFMPEG_LINK_LIBS avformat avcodec swscale avutil)

aux_source_directory(${PROJECT_SOURCE_DIR} RECORDER_FILES)

add_library(${PROJECT_NAME} ${RECORDER_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC capturer v4l2_codecs codec_h264 ${FFMPEG_LINK_LIBS})
